#include "TerminalLauncher.hpp"

#include "ProcessLauncher.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace
{
    std::string executableName(const std::string &command)
    {
        return std::filesystem::path(command).filename().string();
    }

    void appendCommand(TerminalLauncher::Command &argv, const TerminalLauncher::Command &command)
    {
        argv.insert(argv.end(), command.begin(), command.end());
    }

    void addCandidate(std::vector<TerminalLauncher::Command> &candidates, const std::optional<TerminalLauncher::Command> &candidate)
    {
        if (!candidate.has_value() || candidate->empty())
            return;

        const auto candidateName = executableName(candidate->front());
        const auto duplicate = std::any_of(candidates.begin(), candidates.end(), [&](const TerminalLauncher::Command &existing)
                                           { return !existing.empty() && executableName(existing.front()) == candidateName; });
        if (!duplicate)
            candidates.push_back(*candidate);
    }
}

bool TerminalLauncher::open(const std::filesystem::path &workingDir)
{
    return openWithCommand({}, workingDir);
}

bool TerminalLauncher::openWithCommand(const Command &command, const std::filesystem::path &workingDir)
{
    const auto candidates = buildTerminalCandidates(std::getenv("TERMINAL"), command);
    for (const auto &candidate : candidates)
    {
        if (ProcessLauncher::spawnDetached(candidate, workingDir, false))
            return true;
    }

    return false;
}

std::optional<TerminalLauncher::Command> TerminalLauncher::buildTerminalCommand(const Command &terminalCommand, const Command &command)
{
    if (terminalCommand.empty() || terminalCommand.front().empty())
        return std::nullopt;

    Command argv = terminalCommand;
    if (command.empty())
        return argv;

    const auto terminal = executableName(terminalCommand.front());

    if (terminal == "alacritty")
    {
        argv.push_back("-e");
        appendCommand(argv, command);
        return argv;
    }

    if (terminal == "foot" || terminal == "footclient" || terminal == "kitty")
    {
        appendCommand(argv, command);
        return argv;
    }

    if (terminal == "wezterm")
    {
        argv.push_back("start");
        argv.push_back("--");
        appendCommand(argv, command);
        return argv;
    }

    if (terminal == "gnome-terminal")
    {
        argv.push_back("--");
        appendCommand(argv, command);
        return argv;
    }

    if (terminal == "konsole")
    {
        argv.push_back("-e");
        appendCommand(argv, command);
        return argv;
    }

    if (terminal == "xfce4-terminal")
    {
        argv.push_back("-x");
        appendCommand(argv, command);
        return argv;
    }

    if (terminal == "xterm")
    {
        argv.push_back("-e");
        appendCommand(argv, command);
        return argv;
    }

    return std::nullopt;
}

std::vector<TerminalLauncher::Command> TerminalLauncher::buildTerminalCandidates(const char *terminalEnv, const Command &command)
{
    std::vector<Command> candidates;

    if (terminalEnv && *terminalEnv)
        addCandidate(candidates, buildTerminalCommand({terminalEnv}, command));

    const Command defaultTerminals = {
        "alacritty",
        "foot",
        "footclient",
        "kitty",
        "wezterm",
        "gnome-terminal",
        "konsole",
        "xfce4-terminal",
        "xterm",
    };

    for (const auto &terminal : defaultTerminals)
        addCandidate(candidates, buildTerminalCommand({terminal}, command));

    return candidates;
}
