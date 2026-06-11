#include "DependencyReport.hpp"

#include "version/Version.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
    struct Dependency
    {
        std::string name;
        std::string purpose;
    };

    const std::vector<Dependency> &pkgConfigDependencies()
    {
        static const std::vector<Dependency> dependencies = {
            {"hyprtoolkit", "UI framework"},
            {"hyprutils", "memory, logging, CLI, and math utilities"},
            {"pixman-1", "pixel manipulation"},
            {"aquamarine", "display backend"},
            {"libdrm", "direct rendering"},
            {"xkbcommon", "keyboard input and key symbols"},
            {"libavformat", "FFmpeg container parsing"},
            {"libavcodec", "FFmpeg video decoding"},
            {"libswscale", "FFmpeg frame scaling"},
            {"libavutil", "FFmpeg utility APIs"},
            {"gdk-pixbuf-2.0", "image metadata and PNG encoding"},
        };
        return dependencies;
    }

    const std::vector<Dependency> &runtimeDependencies()
    {
        static const std::vector<Dependency> dependencies = {
            {"gio", "trash selected files and directories"},
            {"xdg-open", "open non-text files with desktop defaults"},
            {"nvim", "open text files in a terminal"},
        };
        return dependencies;
    }

    const std::vector<std::string> &terminalCandidates()
    {
        static const std::vector<std::string> terminals = {
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
        return terminals;
    }

    bool isSupportedTerminalCommand(std::string_view command)
    {
        const auto name = std::filesystem::path(std::string(command)).filename().string();
        const auto &terminals = terminalCandidates();
        return std::find(terminals.begin(), terminals.end(), name) != terminals.end();
    }

    std::optional<std::filesystem::path> findInPath(std::string_view executable)
    {
        if (executable.empty())
            return std::nullopt;

        const std::filesystem::path executablePath{std::string(executable)};
        auto isRunnable = [](const std::filesystem::path &path) -> bool
        {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec) || ec)
                return false;
            return access(path.c_str(), X_OK) == 0;
        };

        if (executablePath.has_parent_path())
        {
            if (isRunnable(executablePath))
                return executablePath;
            return std::nullopt;
        }

        const char *pathEnv = std::getenv("PATH");
        if (!pathEnv || *pathEnv == '\0')
            return std::nullopt;

        std::stringstream paths(pathEnv);
        std::string dir;
        while (std::getline(paths, dir, ':'))
        {
            const auto candidate = std::filesystem::path(dir.empty() ? "." : dir) / executablePath;
            if (isRunnable(candidate))
                return candidate;
        }

        return std::nullopt;
    }

    bool pkgConfigModuleExists(std::string_view module)
    {
        const std::string command = "pkg-config --exists " + std::string(module) + " >/dev/null 2>&1";
        const int status = std::system(command.c_str());
        return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    void appendStatus(std::ostringstream &out, bool found, const std::string &name, const std::string &detail)
    {
        out << "  [" << (found ? "found" : "missing") << "] " << name;
        if (!detail.empty())
            out << " - " << detail;
        out << '\n';
    }
}

Debug::SCheckContext Debug::makeSystemCheckContext()
{
    SCheckContext context;
    context.pkgConfigExists = [](std::string_view module)
    {
        return pkgConfigModuleExists(module);
    };
    context.findExecutable = [](std::string_view executable)
    {
        return findInPath(executable);
    };
    context.getEnv = [](std::string_view name) -> std::optional<std::string>
    {
        const std::string key{name};
        const char *value = std::getenv(key.c_str());
        if (!value || *value == '\0')
            return std::nullopt;
        return std::string(value);
    };
    return context;
}

Debug::SDependencyReport Debug::buildDependencyReport(const SCheckContext &context)
{
    SDependencyReport report;
    report.allFound = true;

    std::ostringstream out;
    out << "hyprfile dependency report\n";
    out << "hyprfile version: " << Version::PROJECT_VERSION << '\n';

    out << "\nBuild/pkg-config dependencies:\n";
    for (const auto &dependency : pkgConfigDependencies())
    {
        const bool found = context.pkgConfigExists(dependency.name);
        appendStatus(out, found, dependency.name, dependency.purpose);
        report.allFound = report.allFound && found;
    }

    out << "\nRuntime integrations:\n";
    for (const auto &dependency : runtimeDependencies())
    {
        const auto path = context.findExecutable(dependency.name);
        appendStatus(out, path.has_value(), dependency.name, path ? path->string() : dependency.purpose);
        report.allFound = report.allFound && path.has_value();
    }

    out << "\nTerminal support:\n";
    const auto terminalEnv = context.getEnv("TERMINAL");
    if (terminalEnv && !terminalEnv->empty())
    {
        const auto terminalPath = context.findExecutable(*terminalEnv);
        appendStatus(out, terminalPath.has_value(), "$TERMINAL=" + *terminalEnv, terminalPath ? terminalPath->string() : "not found");
    }
    else
    {
        out << "  $TERMINAL is not set\n";
    }

    bool foundTerminal = false;
    for (const auto &terminal : terminalCandidates())
    {
        const auto path = context.findExecutable(terminal);
        appendStatus(out, path.has_value(), terminal, path ? path->string() : "terminal candidate");
        foundTerminal = foundTerminal || path.has_value();
    }

    if (terminalEnv && !terminalEnv->empty())
    {
        const auto terminalPath = context.findExecutable(*terminalEnv);
        foundTerminal = foundTerminal || (terminalPath.has_value() && isSupportedTerminalCommand(*terminalEnv));
    }

    out << "  Supported terminal: " << (foundTerminal ? "found" : "missing") << '\n';
    report.allFound = report.allFound && foundTerminal;

    report.text = out.str();
    return report;
}
