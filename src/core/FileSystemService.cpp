#include "FileSystemService.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace
{
    bool isHiddenName(const std::string& name)
    {
        return !name.empty() && name.front() == '.';
    }
}

std::vector<FileSystemService::SFileEntry> FileSystemService::listDirectory(const std::filesystem::path &dirPath) const
{
    std::vector<SFileEntry> entries;

    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath))
        return entries;

    try
    {
        for (const auto &entry : std::filesystem::directory_iterator(dirPath))
        {
            try
            {
                const auto path = entry.path();
                const auto name = path.filename().string();
                if (!showHiddenFiles_ && isHiddenName(name))
                    continue;

                SFileEntry fileEntry;
                fileEntry.path = path;
                fileEntry.name = name;
                fileEntry.isDirectory = entry.is_directory();
                entries.push_back(fileEntry);
            }
            catch (const std::filesystem::filesystem_error &)
            {
                // Skip entries we can't stat
            }
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        // Permission denied or other error opening the directory
        return entries;
    }

    std::sort(entries.begin(), entries.end(), [](const SFileEntry &a, const SFileEntry &b)
              {
        if (a.isDirectory != b.isDirectory)
            return a.isDirectory > b.isDirectory;
        return a.name < b.name; });

    return entries;
}

bool FileSystemService::toggleHiddenFiles()
{
    showHiddenFiles_ = !showHiddenFiles_;
    return showHiddenFiles_;
}

bool FileSystemService::showHiddenFiles() const
{
    return showHiddenFiles_;
}

void FileSystemService::setShowHiddenFiles(bool showHiddenFiles)
{
    showHiddenFiles_ = showHiddenFiles;
}

bool FileSystemService::trashWithGio(const std::filesystem::path& absPath)
{
    if (!absPath.is_absolute())
    {
        std::cerr << "[hyprfile] trashWithGio: expected absolute path, got '" << absPath << "'\n";
        return false;
    }

    std::string pathUtf8;
    try
    {
        pathUtf8 = absPath.string();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[hyprfile] trashWithGio: failed to convert path to UTF-8 for '" << absPath
                  << "': " << e.what() << "\n";
        return false;
    }

    std::vector<std::string> args{ "gio", "trash", pathUtf8 };
    std::vector<char*> argv{ args[0].data(), args[1].data(), args[2].data(), nullptr };

    posix_spawnattr_t attr;
    bool attrInitialized = false;

    int rc = posix_spawnattr_init(&attr);
    if (rc != 0)
    {
        std::cerr << "[hyprfile] trashWithGio: posix_spawnattr_init failed for '" << pathUtf8
                  << "' rc=" << rc << " (" << std::strerror(rc) << ")\n";
        return false;
    }

    attrInitialized = true;

    int flags = 0;

#ifdef POSIX_SPAWN_CLOEXEC_DEFAULT
    flags = POSIX_SPAWN_CLOEXEC_DEFAULT;
#endif
    // Fallback without CLOEXEC: minimal FD leak risk is acceptable per spec.

    rc = posix_spawnattr_setflags(&attr, flags);
    if (rc != 0)
    {
        std::cerr << "[hyprfile] trashWithGio: posix_spawnattr_setflags failed for '" << pathUtf8
                  << "' rc=" << rc << " (" << std::strerror(rc) << ")\n";
        posix_spawnattr_destroy(&attr);
        return false;
    }

    pid_t pid = 0;
    rc = posix_spawnp(&pid, argv[0], nullptr, &attr, argv.data(), environ);

    if (attrInitialized)
    {
        posix_spawnattr_destroy(&attr);
    }

    if (rc != 0)
    {
        std::cerr << "[hyprfile] trashWithGio: posix_spawnp failed for '" << pathUtf8
                  << "' rc=" << rc << " (" << std::strerror(rc) << ")\n";
        return false;
    }

    int status = 0;
    pid_t waited = 0;

    // Blocking call: wait for `gio trash` to finish.
    do
    {
        waited = waitpid(pid, &status, 0);
    } while (waited == -1 && errno == EINTR);

    if (waited == -1)
    {
        if (errno == ECHILD)
        {
            std::cerr << "[hyprfile] trashWithGio: waitpid reported ECHILD for '" << pathUtf8
                      << "'; status unknown, assuming success\n";
            return true;
        }

        std::cerr << "[hyprfile] trashWithGio: waitpid failed for '" << pathUtf8 << "' errno=" << errno << "\n";
        return false;
    }

    if (WIFEXITED(status))
    {
        const int exitCode = WEXITSTATUS(status);
        if (exitCode != 0)
        {
            std::cerr << "[hyprfile] trashWithGio: gio exited with code " << exitCode << " for '" << pathUtf8 << "'\n";
            return false;
        }

        return true;
    }

    if (WIFSIGNALED(status))
    {
        std::cerr << "[hyprfile] trashWithGio: gio terminated by signal " << WTERMSIG(status)
                  << " for '" << pathUtf8 << "'\n";
        return false;
    }

    std::cerr << "[hyprfile] trashWithGio: gio exited with unknown status for '" << pathUtf8 << "'\n";
    
    return false;
}
