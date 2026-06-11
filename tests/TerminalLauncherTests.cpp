#include <gtest/gtest.h>

#include "../src/core/TerminalLauncher.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace
{
    using Command = TerminalLauncher::Command;

    void expectCommand(const Command &terminal, const Command &expected)
    {
        auto built = TerminalLauncher::buildTerminalCommand(terminal, {"nvim", "file.txt"});
        ASSERT_TRUE(built.has_value());
        EXPECT_EQ(*built, expected);
    }

    std::size_t findTerminal(const std::vector<Command> &candidates, const std::string &terminal)
    {
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            if (!candidates[i].empty() && candidates[i][0] == terminal)
                return i;
        }

        return candidates.size();
    }
}

TEST(TerminalLauncherTests, BuildsKnownTerminalCommandSyntax)
{
    expectCommand({"alacritty"}, {"alacritty", "-e", "nvim", "file.txt"});
    expectCommand({"foot"}, {"foot", "nvim", "file.txt"});
    expectCommand({"footclient"}, {"footclient", "nvim", "file.txt"});
    expectCommand({"kitty"}, {"kitty", "nvim", "file.txt"});
    expectCommand({"wezterm"}, {"wezterm", "start", "--", "nvim", "file.txt"});
    expectCommand({"gnome-terminal"}, {"gnome-terminal", "--", "nvim", "file.txt"});
    expectCommand({"konsole"}, {"konsole", "-e", "nvim", "file.txt"});
    expectCommand({"xfce4-terminal"}, {"xfce4-terminal", "-x", "nvim", "file.txt"});
    expectCommand({"xterm"}, {"xterm", "-e", "nvim", "file.txt"});
}

TEST(TerminalLauncherTests, RecognizesTerminalByExecutableBasename)
{
    auto built = TerminalLauncher::buildTerminalCommand({"/usr/bin/xterm"}, {"nvim", "file.txt"});

    ASSERT_TRUE(built.has_value());
    EXPECT_EQ(*built, Command({"/usr/bin/xterm", "-e", "nvim", "file.txt"}));
}

TEST(TerminalLauncherTests, SkipsUnknownTerminalForInnerCommand)
{
    auto built = TerminalLauncher::buildTerminalCommand({"mystery-terminal"}, {"nvim", "file.txt"});

    EXPECT_FALSE(built.has_value());
}

TEST(TerminalLauncherTests, AllowsUnknownTerminalWithoutInnerCommand)
{
    auto built = TerminalLauncher::buildTerminalCommand({"mystery-terminal"}, {});

    ASSERT_TRUE(built.has_value());
    EXPECT_EQ(*built, Command({"mystery-terminal"}));
}

TEST(TerminalLauncherTests, UsesSupportedTerminalEnvFirst)
{
    auto candidates = TerminalLauncher::buildTerminalCandidates("xterm", {"nvim", "file.txt"});

    ASSERT_FALSE(candidates.empty());
    EXPECT_EQ(candidates.front(), Command({"xterm", "-e", "nvim", "file.txt"}));
}

TEST(TerminalLauncherTests, IgnoresUnknownTerminalEnvForInnerCommand)
{
    auto candidates = TerminalLauncher::buildTerminalCandidates("mystery-terminal", {"nvim", "file.txt"});

    ASSERT_FALSE(candidates.empty());
    EXPECT_NE(candidates.front()[0], "mystery-terminal");
}

TEST(TerminalLauncherTests, TriesFootBeforeFootclientByDefault)
{
    auto candidates = TerminalLauncher::buildTerminalCandidates(nullptr, {"nvim", "file.txt"});
    const auto footIndex = findTerminal(candidates, "foot");
    const auto footclientIndex = findTerminal(candidates, "footclient");

    ASSERT_NE(footIndex, candidates.size());
    ASSERT_NE(footclientIndex, candidates.size());
    EXPECT_LT(footIndex, footclientIndex);
}
