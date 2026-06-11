#include "FileLauncher.hpp"

#include "ProcessLauncher.hpp"
#include "TerminalLauncher.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    bool hasExecPerm(const std::filesystem::path &path)
    {
        std::error_code ec;
        auto perms = std::filesystem::status(path, ec).permissions();
        if (ec)
            return false;

        using P = std::filesystem::perms;
        return (perms & P::owner_exec) != P::none ||
               (perms & P::group_exec) != P::none ||
               (perms & P::others_exec) != P::none;
    }

}

bool FileLauncher::open(const std::filesystem::path &path, FileItem::FileItemType type)
{
    if (path.empty())
        return false;

    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return false;

    const auto parentDir = path.parent_path();

    if (type == FileItem::FileItemType::Text)
    {
        const bool canChdir = !parentDir.empty();
        const std::string fileArg = canChdir ? path.filename().string() : path.string();
        const std::filesystem::path cwd = canChdir ? parentDir : std::filesystem::path{};

        return TerminalLauncher::openWithCommand({"nvim", fileArg}, cwd);
    }

    if (type == FileItem::FileItemType::Binary)
    {
        if (!hasExecPerm(path))
        {
            std::cout << "Refusing to execute non-executable file: " << path << "\n";
            return false;
        }

        return ProcessLauncher::spawnDetached({path.string()}, parentDir);
    }

    // Delegate to the desktop's default handler for other types.
    return ProcessLauncher::spawnDetached({"xdg-open", path.string()}, parentDir);
}
