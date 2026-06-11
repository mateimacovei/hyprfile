#include "FileOperationClipboard.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unistd.h>

namespace
{
    const char* operationName(FileOperationClipboard::Operation operation)
    {
        switch (operation)
        {
            case FileOperationClipboard::Operation::Copy:
                return "copy";
            case FileOperationClipboard::Operation::Cut:
                return "cut";
        }

        return "";
    }

    std::optional<FileOperationClipboard::Operation> parseOperation(const std::string& value)
    {
        if (value == "copy")
            return FileOperationClipboard::Operation::Copy;
        if (value == "cut")
            return FileOperationClipboard::Operation::Cut;
        return std::nullopt;
    }

    std::filesystem::path absolutePath(const std::filesystem::path& path)
    {
        std::error_code ec;
        auto absolute = std::filesystem::absolute(path, ec);
        if (ec)
            return path;
        return absolute.lexically_normal();
    }
}

FileOperationClipboard::FileOperationClipboard()
    : stateFile_(defaultStateFile())
{
}

FileOperationClipboard::FileOperationClipboard(std::filesystem::path stateFile)
    : stateFile_(std::move(stateFile))
{
}

std::optional<FileOperationClipboard::State> FileOperationClipboard::read() const
{
    std::ifstream file(stateFile_, std::ios::binary);
    if (!file)
        return std::nullopt;

    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.empty())
        return std::nullopt;

    const auto newline = bytes.find('\n');
    if (newline == std::string::npos)
    {
        std::cerr << "[hyprfile] file operation clipboard: missing operation line in '" << stateFile_ << "'\n";
        return std::nullopt;
    }

    const auto operation = parseOperation(bytes.substr(0, newline));
    if (!operation)
    {
        std::cerr << "[hyprfile] file operation clipboard: invalid operation in '" << stateFile_ << "'\n";
        return std::nullopt;
    }

    std::vector<std::filesystem::path> sources;
    std::size_t start = newline + 1;
    while (start < bytes.size())
    {
        const auto end = bytes.find('\0', start);
        const auto count = (end == std::string::npos) ? bytes.size() - start : end - start;
        if (count > 0)
            sources.emplace_back(bytes.substr(start, count));

        if (end == std::string::npos)
            break;
        start = end + 1;
    }

    return State{*operation, std::move(sources)};
}

bool FileOperationClipboard::write(Operation operation, const std::vector<std::filesystem::path>& sources) const
{
    if (sources.empty())
    {
        std::cerr << "[hyprfile] file operation clipboard: refusing to write empty source list\n";
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(stateFile_.parent_path(), ec);
    if (ec)
    {
        std::cerr << "[hyprfile] file operation clipboard: failed to create state directory '"
                  << stateFile_.parent_path() << "': " << ec.message() << "\n";
        return false;
    }

    std::filesystem::permissions(stateFile_.parent_path(), std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, ec);
    if (ec)
    {
        std::cerr << "[hyprfile] file operation clipboard: failed to restrict state directory '"
                  << stateFile_.parent_path() << "': " << ec.message() << "\n";
        return false;
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto tempFile = std::filesystem::path(stateFile_.string() + ".tmp." + std::to_string(getpid()) + "." + std::to_string(stamp));

    {
        std::ofstream file(tempFile, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            std::cerr << "[hyprfile] file operation clipboard: failed to open temp state file '" << tempFile << "'\n";
            return false;
        }

        file << operationName(operation) << '\n';
        for (const auto& source : sources)
        {
            const auto absolute = absolutePath(source).string();
            file.write(absolute.data(), static_cast<std::streamsize>(absolute.size()));
            file.put('\0');
        }

        if (!file)
        {
            std::cerr << "[hyprfile] file operation clipboard: failed writing temp state file '" << tempFile << "'\n";
            std::filesystem::remove(tempFile, ec);
            return false;
        }
    }

    std::filesystem::permissions(tempFile, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace, ec);
    if (ec)
    {
        std::cerr << "[hyprfile] file operation clipboard: failed to restrict temp state file '" << tempFile
                  << "': " << ec.message() << "\n";
        std::filesystem::remove(tempFile, ec);
        return false;
    }

    std::filesystem::rename(tempFile, stateFile_, ec);
    if (ec)
    {
        std::cerr << "[hyprfile] file operation clipboard: failed to publish state file '" << stateFile_
                  << "': " << ec.message() << "\n";
        std::filesystem::remove(tempFile, ec);
        return false;
    }

    return true;
}

bool FileOperationClipboard::clear() const
{
    std::error_code ec;
    std::filesystem::remove(stateFile_, ec);
    if (!ec)
        return true;

    const auto removeError = ec.message();
    std::ofstream file(stateFile_, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        std::cerr << "[hyprfile] file operation clipboard: failed to clear state file '" << stateFile_
                  << "': " << removeError << "\n";
        return false;
    }

    file.close();
    if (!file)
    {
        std::cerr << "[hyprfile] file operation clipboard: failed to truncate state file '" << stateFile_ << "'\n";
        return false;
    }

    std::filesystem::permissions(stateFile_, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace, ec);

    return true;
}

const std::filesystem::path& FileOperationClipboard::stateFile() const
{
    return stateFile_;
}

std::filesystem::path FileOperationClipboard::defaultStateFile()
{
    if (const char* runtimeDir = std::getenv("XDG_RUNTIME_DIR"); runtimeDir && runtimeDir[0] != '\0')
        return std::filesystem::path(runtimeDir) / "hyprfile" / "file-operation-clipboard";

    std::error_code ec;
    auto tempDir = std::filesystem::temp_directory_path(ec);
    if (ec)
        tempDir = "/tmp";

    return tempDir / ("hyprfile-" + std::to_string(getuid())) / "file-operation-clipboard";
}
