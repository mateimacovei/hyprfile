#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "../src/debug/DependencyReport.hpp"

namespace
{
    Debug::SCheckContext makeContext(std::unordered_set<std::string> packages,
                                     std::unordered_map<std::string, std::filesystem::path> commands,
                                     std::optional<std::string> terminalEnv = std::nullopt)
    {
        Debug::SCheckContext context;
        context.pkgConfigExists = [packages = std::move(packages)](std::string_view package) mutable
        {
            return packages.contains(std::string(package));
        };
        context.findExecutable = [commands = std::move(commands)](std::string_view command) mutable -> std::optional<std::filesystem::path>
        {
            auto it = commands.find(std::string(command));
            if (it == commands.end())
                return std::nullopt;
            return it->second;
        };
        context.getEnv = [terminalEnv = std::move(terminalEnv)](std::string_view name) mutable -> std::optional<std::string>
        {
            if (name == "TERMINAL")
                return terminalEnv;
            return std::nullopt;
        };
        return context;
    }
}

TEST(DebugReportTests, MarksMissingPkgConfigDependency)
{
    auto context = makeContext({"hyprtoolkit", "hyprutils", "pixman-1", "aquamarine", "xkbcommon",
                                "libavformat", "libavcodec", "libswscale", "libavutil", "gdk-pixbuf-2.0"},
                               {{"gio", "/usr/bin/gio"}, {"xdg-open", "/usr/bin/xdg-open"}, {"nvim", "/usr/bin/nvim"}, {"kitty", "/usr/bin/kitty"}});

    const auto report = Debug::buildDependencyReport(context);

    EXPECT_FALSE(report.allFound);
    EXPECT_NE(report.text.find("[missing] libdrm"), std::string::npos);
    EXPECT_NE(report.text.find("[found] hyprtoolkit"), std::string::npos);
}

TEST(DebugReportTests, HeaderUsesHyprfileName)
{
    auto context = makeContext({"hyprtoolkit", "hyprutils", "pixman-1", "aquamarine", "libdrm", "xkbcommon",
                                "libavformat", "libavcodec", "libswscale", "libavutil", "gdk-pixbuf-2.0"},
                               {{"gio", "/usr/bin/gio"}, {"xdg-open", "/usr/bin/xdg-open"}, {"nvim", "/usr/bin/nvim"}, {"kitty", "/usr/bin/kitty"}});

    const auto report = Debug::buildDependencyReport(context);

    EXPECT_NE(report.text.find("hyprfile dependency report"), std::string::npos);
    EXPECT_NE(report.text.find("hyprfile version: "), std::string::npos);
}

TEST(DebugReportTests, RequiresOnlyOneSupportedTerminal)
{
    auto context = makeContext({"hyprtoolkit", "hyprutils", "pixman-1", "aquamarine", "libdrm", "xkbcommon",
                                "libavformat", "libavcodec", "libswscale", "libavutil", "gdk-pixbuf-2.0"},
                               {{"gio", "/usr/bin/gio"}, {"xdg-open", "/usr/bin/xdg-open"}, {"nvim", "/usr/bin/nvim"}, {"foot", "/usr/bin/foot"}});

    const auto report = Debug::buildDependencyReport(context);

    EXPECT_TRUE(report.allFound);
    EXPECT_NE(report.text.find("Supported terminal: found"), std::string::npos);
    EXPECT_NE(report.text.find("[found] foot"), std::string::npos);
    EXPECT_NE(report.text.find("[missing] alacritty"), std::string::npos);
}

TEST(DebugReportTests, SupportedTerminalEnvSatisfiesTerminalRequirement)
{
    auto context = makeContext({"hyprtoolkit", "hyprutils", "pixman-1", "aquamarine", "libdrm", "xkbcommon",
                                "libavformat", "libavcodec", "libswscale", "libavutil", "gdk-pixbuf-2.0"},
                               {{"gio", "/usr/bin/gio"},
                                {"xdg-open", "/usr/bin/xdg-open"},
                                {"nvim", "/usr/bin/nvim"},
                                {"/opt/kitty/bin/kitty", "/opt/kitty/bin/kitty"}},
                               "/opt/kitty/bin/kitty");

    const auto report = Debug::buildDependencyReport(context);

    EXPECT_TRUE(report.allFound);
    EXPECT_NE(report.text.find("[found] $TERMINAL=/opt/kitty/bin/kitty"), std::string::npos);
    EXPECT_NE(report.text.find("Supported terminal: found"), std::string::npos);
}

TEST(DebugReportTests, MissingRuntimeCommandFailsReport)
{
    auto context = makeContext({"hyprtoolkit", "hyprutils", "pixman-1", "aquamarine", "libdrm", "xkbcommon",
                                "libavformat", "libavcodec", "libswscale", "libavutil", "gdk-pixbuf-2.0"},
                               {{"gio", "/usr/bin/gio"}, {"nvim", "/usr/bin/nvim"}, {"kitty", "/usr/bin/kitty"}});

    const auto report = Debug::buildDependencyReport(context);

    EXPECT_FALSE(report.allFound);
    EXPECT_NE(report.text.find("[missing] xdg-open"), std::string::npos);
}
