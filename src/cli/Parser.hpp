#pragma once

#include <filesystem>
#include <expected>

namespace CLI
{
    struct SCLIOptions
    {
        std::filesystem::path cwd;
        bool verbose = false;
        bool debug = false;
    };

    // Parse argv/argc and return options or an error message
    std::expected<SCLIOptions, std::string> parseArguments(int argc, char **argv);
}
