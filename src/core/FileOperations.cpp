#include "FileOperations.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <linux/fs.h>
#include <string>
#include <system_error>
#include <sys/syscall.h>
#include <unordered_set>
#include <unistd.h>

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif

namespace
{
    static constexpr int kMaxConflictAttempts = 10000;

    struct PlannedEntry
    {
        std::filesystem::path originalSource;
        std::filesystem::path source;
        std::filesystem::path destination;
        bool isDirectory = false;
        bool sameParentNoop = false;
    };

    std::filesystem::path absolutePath(const std::filesystem::path& path)
    {
        std::error_code ec;
        const auto absolute = std::filesystem::absolute(path, ec);
        if (ec)
            return path.lexically_normal();
        return absolute.lexically_normal();
    }

    std::filesystem::path normalizedExistingPath(const std::filesystem::path& path)
    {
        std::error_code ec;
        const auto normalized = std::filesystem::weakly_canonical(path, ec);
        if (ec)
            return absolutePath(path);
        return normalized.lexically_normal();
    }

    std::string pathKey(const std::filesystem::path& path)
    {
        return path.lexically_normal().string();
    }

    bool pathOccupied(const std::filesystem::path& path, std::string& error)
    {
        std::error_code ec;
        const auto status = std::filesystem::symlink_status(path, ec);
        if (ec)
        {
            if (ec == std::errc::no_such_file_or_directory)
                return false;

            error = "failed checking path '" + path.string() + "': " + ec.message();
            return true;
        }

        return status.type() != std::filesystem::file_type::not_found;
    }

    bool isPathInside(const std::filesystem::path& child, const std::filesystem::path& parent)
    {
        auto childIt = child.begin();
        auto parentIt = parent.begin();
        for (; parentIt != parent.end(); ++parentIt, ++childIt)
        {
            if (childIt == child.end() || *childIt != *parentIt)
                return false;
        }

        return true;
    }

    std::filesystem::path copyName(const std::filesystem::path& filename, bool isDirectory, int copyNumber)
    {
        const std::string suffix = copyNumber == 1 ? " copy" : " copy " + std::to_string(copyNumber);
        if (isDirectory)
            return filename.string() + suffix;

        return filename.stem().string() + suffix + filename.extension().string();
    }

    bool generateDestination(const std::filesystem::path& source,
                             const std::filesystem::path& targetDirectory,
                             bool isDirectory,
                             std::unordered_set<std::string>& reservedDestinations,
                             std::filesystem::path& destination,
                             std::string& error)
    {
        const auto filename = source.filename();
        if (filename.empty())
        {
            error = "source has no filename: " + source.string();
            return false;
        }

        for (int attempt = 0; attempt < kMaxConflictAttempts; ++attempt)
        {
            const auto candidateName = attempt == 0 ? filename : copyName(filename, isDirectory, attempt);
            const auto candidate = (targetDirectory / candidateName).lexically_normal();
            const auto candidateKey = pathKey(candidate);

            const bool occupied = pathOccupied(candidate, error);
            if (!error.empty())
                return false;

            if (!occupied && !reservedDestinations.contains(candidateKey))
            {
                reservedDestinations.insert(candidateKey);
                destination = candidate;
                return true;
            }
        }

        error = "could not find non-conflicting destination for '" + source.string() + "'";
        return false;
    }

    bool generateTemporaryDestination(const std::filesystem::path& destination,
                                      std::filesystem::path& temporaryDestination,
                                      std::string& error)
    {
        const auto parent = destination.parent_path();
        const auto filename = destination.filename().string();
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();

        for (int attempt = 0; attempt < kMaxConflictAttempts; ++attempt)
        {
            const auto tempName = "." + filename + ".hyprfile-tmp-" + std::to_string(getpid()) + "-" +
                                  std::to_string(stamp) + "-" + std::to_string(attempt);
            const auto candidate = parent / tempName;
            if (!pathOccupied(candidate, error))
            {
                temporaryDestination = candidate;
                return true;
            }

            if (!error.empty())
                return false;
        }

        error = "could not find temporary destination for '" + destination.string() + "'";
        return false;
    }

    bool copyToDestination(const PlannedEntry& entry, const std::filesystem::path& destination, std::string& error)
    {
        std::error_code ec;
        if (entry.isDirectory)
        {
            std::filesystem::copy(entry.source, destination, std::filesystem::copy_options::recursive, ec);
        }
        else
        {
            std::filesystem::copy_file(entry.source, destination, std::filesystem::copy_options::none, ec);
        }

        if (!ec)
            return true;

        error = "failed to copy '" + entry.source.string() + "' to '" + destination.string() + "': " + ec.message();

        std::error_code cleanupEc;
        std::filesystem::remove_all(destination, cleanupEc);
        return false;
    }

    bool renameNoReplace(const std::filesystem::path& source,
                         const std::filesystem::path& destination,
                         std::error_code& ec);

    bool copyEntry(const PlannedEntry& entry, std::string& error)
    {
        std::filesystem::path temporaryDestination;
        if (!generateTemporaryDestination(entry.destination, temporaryDestination, error))
            return false;

        if (!copyToDestination(entry, temporaryDestination, error))
            return false;

        std::error_code ec;
        renameNoReplace(temporaryDestination, entry.destination, ec);
        if (!ec)
            return true;

        error = "failed to publish copied destination '" + entry.destination.string() + "': " + ec.message();
        std::error_code cleanupEc;
        std::filesystem::remove_all(temporaryDestination, cleanupEc);
        return false;
    }

    bool isFallbackRenameError(const std::error_code& ec)
    {
        return ec == std::errc::cross_device_link ||
               ec == std::errc::operation_not_supported ||
               ec == std::errc::function_not_supported ||
               ec.value() == EXDEV;
    }

    bool renameNoReplace(const std::filesystem::path& source,
                         const std::filesystem::path& destination,
                         std::error_code& ec)
    {
#ifdef SYS_renameat2
        if (syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD, destination.c_str(), RENAME_NOREPLACE) == 0)
        {
            ec.clear();
            return true;
        }

        ec = std::error_code(errno, std::generic_category());
        return false;
#else
        ec = std::make_error_code(std::errc::function_not_supported);
        return false;
#endif
    }

    bool removeSourceAfterCopy(const PlannedEntry& entry, std::string& error)
    {
        std::error_code ec;
        if (entry.isDirectory)
            std::filesystem::remove_all(entry.source, ec);
        else
            std::filesystem::remove(entry.source, ec);

        if (!ec)
            return true;

        error = "failed to remove original source '" + entry.source.string() + "' after copy: " + ec.message();
        std::error_code cleanupEc;
        std::filesystem::remove_all(entry.destination, cleanupEc);
        return false;
    }

    bool moveEntry(const PlannedEntry& entry, std::string& error)
    {
        std::error_code ec;
        renameNoReplace(entry.source, entry.destination, ec);
        if (!ec)
            return true;

        if (!isFallbackRenameError(ec))
        {
            error = "failed to move '" + entry.source.string() + "' to '" + entry.destination.string() + "': " + ec.message();
            return false;
        }

        std::string copyError;
        if (!copyEntry(entry, copyError))
        {
            error = "failed fallback move copy for '" + entry.source.string() + "': " + copyError;
            return false;
        }

        return removeSourceAfterCopy(entry, error);
    }

    FileOperations::PasteResult preflight(const FileOperationClipboard::State& state,
                                          const std::filesystem::path& targetDirectory,
                                          std::vector<PlannedEntry>& planned)
    {
        FileOperations::PasteResult result;
        result.remainingSources = state.sources;

        if (state.sources.empty())
        {
            result.preflightFailed = true;
            result.error = "clipboard source list is empty";
            return result;
        }

        const auto target = absolutePath(targetDirectory);
        std::error_code ec;
        if (!std::filesystem::exists(target, ec) || ec)
        {
            result.preflightFailed = true;
            result.error = "target directory does not exist: " + target.string();
            return result;
        }

        if (!std::filesystem::is_directory(target, ec) || ec)
        {
            result.preflightFailed = true;
            result.error = "target path is not a directory: " + target.string();
            return result;
        }

        const auto normalizedTarget = normalizedExistingPath(target);
        std::unordered_set<std::string> seenSources;
        std::unordered_set<std::string> reservedDestinations;
        planned.reserve(state.sources.size());

        for (const auto& originalSource : state.sources)
        {
            const auto source = absolutePath(originalSource);
            if (!std::filesystem::exists(source, ec) || ec)
            {
                result.preflightFailed = true;
                result.error = "source path does not exist: " + source.string();
                return result;
            }

            const auto normalizedSource = normalizedExistingPath(source);
            if (!seenSources.insert(pathKey(normalizedSource)).second)
            {
                result.preflightFailed = true;
                result.error = "duplicate source path in clipboard: " + source.string();
                return result;
            }

            const bool isDirectory = std::filesystem::is_directory(source, ec);
            if (ec)
            {
                result.preflightFailed = true;
                result.error = "failed to stat source path: " + source.string();
                return result;
            }

            const auto normalizedParent = normalizedExistingPath(source.parent_path());
            const bool sameParentNoop = state.operation == FileOperationClipboard::Operation::Cut && normalizedParent == normalizedTarget;

            if (isDirectory && !sameParentNoop && isPathInside(normalizedTarget, normalizedSource))
            {
                result.preflightFailed = true;
                result.error = "cannot paste a directory into itself: " + source.string();
                return result;
            }

            std::filesystem::path destination;
            if (sameParentNoop)
            {
                destination = source;
            }
            else if (!generateDestination(source, target, isDirectory, reservedDestinations, destination, result.error))
            {
                result.preflightFailed = true;
                return result;
            }

            planned.push_back({originalSource, source, destination, isDirectory, sameParentNoop});
        }

        result.remainingSources.clear();
        return result;
    }
}

FileOperations::PasteResult FileOperations::paste(const FileOperationClipboard::State& state,
                                                  const std::filesystem::path& targetDirectory)
{
    std::vector<PlannedEntry> planned;
    auto result = preflight(state, targetDirectory, planned);
    if (result.preflightFailed)
        return result;

    for (std::size_t i = 0; i < planned.size(); ++i)
    {
        const auto& entry = planned[i];
        bool entrySucceeded = false;

        if (entry.sameParentNoop)
        {
            entrySucceeded = true;
        }
        else if (state.operation == FileOperationClipboard::Operation::Copy)
        {
            entrySucceeded = copyEntry(entry, result.error);
        }
        else
        {
            entrySucceeded = moveEntry(entry, result.error);
        }

        if (!entrySucceeded)
        {
            result.success = false;
            result.preflightFailed = false;
            result.remainingSources.assign(state.sources.begin() + static_cast<std::ptrdiff_t>(i), state.sources.end());
            std::cerr << "[hyprfile] paste: failed source '" << entry.source << "': " << result.error << "\n";
            return result;
        }

        result.destinations.push_back(entry.destination);
    }

    result.success = true;
    result.remainingSources.clear();
    return result;
}
