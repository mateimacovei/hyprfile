#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class TerminalLauncher
{
public:
    using Command = std::vector<std::string>;

    static bool open(const std::filesystem::path &workingDir);
    static bool openWithCommand(const Command &command, const std::filesystem::path &workingDir);

    static std::optional<Command> buildTerminalCommand(const Command &terminalCommand, const Command &command);
    static std::vector<Command> buildTerminalCandidates(const char *terminalEnv, const Command &command);
};
