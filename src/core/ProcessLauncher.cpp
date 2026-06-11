#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "ProcessLauncher.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern "C" char **environ;

bool ProcessLauncher::spawnDetached(const std::vector<std::string> &args,
                                    const std::filesystem::path &workingDir,
                                    bool logOnError)
{
    if (args.empty())
        return false;

    std::vector<char *> argv;
    argv.reserve(args.size() + 1);
    for (const auto &arg : args)
        argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    int rc = posix_spawn_file_actions_init(&actions);
    if (rc != 0)
    {
        if (logOnError)
            std::cout << "Failed to initialize spawn actions for '" << args[0] << "': " << std::strerror(rc) << "\n";
        return false;
    }

    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDWR, 0);
    posix_spawn_file_actions_adddup2(&actions, STDIN_FILENO, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, STDIN_FILENO, STDERR_FILENO);

#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    if (!workingDir.empty())
    {
        rc = posix_spawn_file_actions_addchdir_np(&actions, workingDir.c_str());
        if (rc != 0)
        {
            if (logOnError)
                std::cout << "Failed to set working directory for '" << args[0] << "': " << std::strerror(rc) << "\n";
            posix_spawn_file_actions_destroy(&actions);
            return false;
        }
    }
#endif

    posix_spawnattr_t attr;
    rc = posix_spawnattr_init(&attr);
    if (rc != 0)
    {
        if (logOnError)
            std::cout << "Failed to initialize spawn attributes for '" << args[0] << "': " << std::strerror(rc) << "\n";
        posix_spawn_file_actions_destroy(&actions);
        return false;
    }

    short flags = 0;
    flags |= POSIX_SPAWN_SETPGROUP;
    posix_spawnattr_setflags(&attr, flags);
    posix_spawnattr_setpgroup(&attr, 0);

    pid_t pid = -1;
    rc = posix_spawnp(&pid, argv[0], &actions, &attr, argv.data(), environ);

    posix_spawnattr_destroy(&attr);
    posix_spawn_file_actions_destroy(&actions);

    if (rc != 0)
    {
        if (logOnError)
            std::cout << "Failed to spawn process for '" << args[0] << "': " << std::strerror(rc) << "\n";
        return false;
    }

    std::thread reaper{[pid]()
                       {
                           int status = 0;
                           while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
                           {
                           }
                       }};
    reaper.detach();

    return true;
}
