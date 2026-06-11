#pragma once

#include <filesystem>
#include <string>
#include <vector>

class ProcessLauncher
{
public:
    static bool spawnDetached(const std::vector<std::string> &args,
                              const std::filesystem::path &workingDir = {},
                              bool logOnError = true);
};
