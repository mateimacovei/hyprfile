#pragma once

#include <filesystem>
#include <optional>
#include <vector>

class FileOperationClipboard
{
public:
    enum class Operation
    {
        Copy,
        Cut,
    };

    struct State
    {
        Operation operation;
        std::vector<std::filesystem::path> sources;
    };

    FileOperationClipboard();
    explicit FileOperationClipboard(std::filesystem::path stateFile);

    std::optional<State> read() const;
    bool write(Operation operation, const std::vector<std::filesystem::path>& sources) const;
    bool clear() const;

    const std::filesystem::path& stateFile() const;
    static std::filesystem::path defaultStateFile();

private:
    std::filesystem::path stateFile_;
};
