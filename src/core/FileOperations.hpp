#pragma once

#include "FileOperationClipboard.hpp"

#include <filesystem>
#include <string>
#include <vector>

class FileOperations
{
public:
    struct PasteResult
    {
        bool success = false;
        bool preflightFailed = false;
        std::vector<std::filesystem::path> destinations;
        std::vector<std::filesystem::path> remainingSources;
        std::string error;
    };

    static PasteResult paste(const FileOperationClipboard::State& state,
                             const std::filesystem::path& targetDirectory);
};
