#include "Parser.hpp"

#include <hyprutils/cli/ArgumentParser.hpp>

#include <filesystem>
#include <optional>
#include <vector>
#include <string>
#include <cstdlib>

namespace
{

    std::filesystem::path normalizeCwd(std::filesystem::path candidate)
    {
        if (candidate.empty())
            return candidate;

        auto normalizedCandidate = candidate.lexically_normal();
        auto pathString = normalizedCandidate.string();
        auto isSeparator = [](char c)
        { return c == '/' || c == '\\'; };
        while (pathString.size() > 1 && isSeparator(pathString.back()))
        {
            const auto root = std::filesystem::path(pathString).root_path().string();
            if (!root.empty() && root == pathString)
                break;

            pathString.pop_back();
        }

        std::filesystem::path normalized(pathString);
        if (!normalized.empty() && std::filesystem::exists(normalized) && !std::filesystem::is_directory(normalized))
        {
            auto parent = normalized.parent_path();
            if (parent.empty())
                parent = ".";
            normalized = std::move(parent);
        }

        return normalized;
    }

    std::filesystem::path getHomeDirectory()
    {
        if (const char *home = std::getenv("HOME"); home && *home != '\0')
            return std::filesystem::path(home);
        return std::filesystem::current_path();
    }

    std::expected<std::filesystem::path, std::string> resolveStartingDirectory(const std::optional<std::string> &provided)
    {
        std::filesystem::path candidate;
        if (provided.has_value())
        {
            if (provided->empty())
                return std::unexpected(std::string("Provided directory path is empty"));

            candidate = std::filesystem::absolute(std::filesystem::path(*provided));
        }
        else
        {
            candidate = getHomeDirectory();
        }

        if (candidate.empty())
            return std::unexpected(std::string("Resolved directory path is empty"));

        const auto normalized = normalizeCwd(std::move(candidate));
        if (normalized.empty())
            return std::unexpected(std::string("Resolved directory path is invalid"));

        return normalized;
    }

}

std::expected<CLI::SCLIOptions, std::string> CLI::parseArguments(int argc, char **argv)
{
    // First, detect whether any named (dash-prefixed) options were supplied.
    bool anyDash = false;
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] && argv[i][0] == '-')
        {
            anyDash = true;
            break;
        }
    }

    // If no dash-prefixed args were given, treat the first positional (if any) as the cwd
    // and do not invoke the argument parser (which can reject stray positionals).
    if (!anyDash)
    {
        CLI::SCLIOptions out;

        std::optional<std::string> positional;
        if (argc > 1 && argv[1])
            positional = argv[1];

        auto cwdResult = resolveStartingDirectory(positional);
        if (!cwdResult)
            return std::unexpected(cwdResult.error());

        out.cwd = *cwdResult;
        out.verbose = false;
        out.debug = false;
        return out;
    }

    // Build a filtered argv list that contains only the program name and the named options
    // with their values. This prevents the underlying parser from failing on stray
    // positional tokens when the user mixes positional and named args.
    std::vector<const char *> filtered;
    filtered.reserve(argc);
    filtered.push_back(argv[0]);
    for (int i = 1; i < argc; ++i)
    {
        const char *a = argv[i];
        if (!a)
            continue;
        if (a[0] != '-')
            continue; // skip positional tokens

        // include the flag
        filtered.push_back(a);

        // if the next token exists and does not start with '-', treat it as the value
        if (i + 1 < argc && argv[i + 1] && argv[i + 1][0] != '-')
        {
            filtered.push_back(argv[i + 1]);
            ++i;
        }
    }

    Hyprutils::CLI::CArgumentParser parser({filtered.data(), filtered.size()});
    if (auto r = parser.registerBoolOption("verbose", "v", "Enable hyprtoolkit debug output when present"); !r)
        return std::unexpected(std::string("Failed to register --verbose: ") + r.error());
    if (auto r = parser.registerBoolOption("debug", "", "Print dependency diagnostics and exit"); !r)
        return std::unexpected(std::string("Failed to register --debug: ") + r.error());
    if (auto r = parser.registerStringOption("dir", "d", "Working directory to start in"); !r)
        return std::unexpected(std::string("Failed to register --dir: ") + r.error());

    if (auto r = parser.parse(); !r)
        return std::unexpected(std::string("CLI parse error: ") + r.error());

    const bool suppliedVerbose = parser.getBool("verbose").has_value();
    const bool suppliedDebug = parser.getBool("debug").has_value();
    const bool suppliedDirNamed = parser.getString("dir").has_value();
    const bool anyNamedProvided = suppliedVerbose || suppliedDebug || suppliedDirNamed;

    CLI::SCLIOptions out;

    if (anyNamedProvided)
    {
        if (!suppliedDirNamed && !suppliedDebug)
            return std::unexpected(std::string("When using named options you must also provide -d/--dir to set working directory."));

        if (suppliedDirNamed)
        {
            auto sv = parser.getString("dir");
            if (sv && !sv->empty())
            {
                std::string svStr = std::string(*sv);
                auto cwdResult = resolveStartingDirectory(svStr);
                if (!cwdResult)
                    return std::unexpected(std::string("Invalid directory provided to -d/--dir"));

                out.cwd = *cwdResult;
            }
            else
            {
                return std::unexpected(std::string("Invalid directory provided to -d/--dir"));
            }
        }
        else
        {
            auto cwdResult = resolveStartingDirectory(std::nullopt);
            if (!cwdResult)
                return std::unexpected(cwdResult.error());

            out.cwd = *cwdResult;
        }
    }
    else
    {
        if (argc > 1)
            out.cwd = normalizeCwd(std::filesystem::path(argv[1]));
        else
            out.cwd = normalizeCwd(std::filesystem::current_path());
    }

    if (auto vb = parser.getBool("verbose"); vb && *vb)
        out.verbose = true;
    else
        out.verbose = false;

    if (auto debug = parser.getBool("debug"); debug && *debug)
        out.debug = true;
    else
        out.debug = false;

    return out;
}
