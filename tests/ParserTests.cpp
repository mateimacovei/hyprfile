#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <cstdlib>

#include "../src/cli/Parser.hpp"

namespace
{

    // Helper to create argv array
    static std::vector<char *> make_argv(const std::vector<std::string> &parts)
    {
        std::vector<char *> argv;
        argv.reserve(parts.size());
        for (auto &p : parts)
            argv.push_back(const_cast<char *>(p.c_str()));
        return argv;
    }

    struct TempFileGuard
    {
        std::filesystem::path path;

        explicit TempFileGuard(std::filesystem::path p) : path(std::move(p))
        {
            std::ofstream ofs(path);
            ofs << "test";
        }

        ~TempFileGuard()
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    };

    std::string ensureTrailingSlash(const std::filesystem::path &dir)
    {
        auto result = dir.string();
        if (result.empty() || (result.back() != '/' && result.back() != '\\'))
            result.push_back('/');
        return result;
    }

    std::filesystem::path getExpectedHomeDirectory()
    {
        if (const char *home = std::getenv("HOME"); home && *home != '\0')
            return std::filesystem::path(home);
        return std::filesystem::current_path();
    }

}

TEST(ParserTests, NoArgs_UsesHomeDirectory)
{
    std::filesystem::path expectedHome = getExpectedHomeDirectory();
    std::vector<std::string> parts = {"prog"};
    auto argv = make_argv(parts);

    auto res = CLI::parseArguments(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->cwd, expectedHome);
    EXPECT_FALSE(res->verbose);
}

TEST(ParserTests, PositionalDir_OverridesCwd)
{
    std::string tmp = "/tmp";
    std::vector<std::string> parts = {"prog", tmp};
    auto argv = make_argv(parts);

    auto res = CLI::parseArguments(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->cwd, std::filesystem::path(tmp));
    EXPECT_FALSE(res->verbose);
}

TEST(ParserTests, NamedDirAndVerbose_Succeeds)
{
    std::string tmp = "/tmp";
    std::vector<std::string> parts = {"prog", "-v", "-d", tmp};
    auto argv = make_argv(parts);

    auto res = CLI::parseArguments(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->cwd, std::filesystem::path(tmp));
    EXPECT_TRUE(res->verbose);
}

TEST(ParserTests, NamedVerboseWithoutDir_Fails)
{
    std::vector<std::string> parts = {"prog", "-v"};
    auto argv = make_argv(parts);

    auto res = CLI::parseArguments(static_cast<int>(argv.size()), argv.data());
    ASSERT_FALSE(res.has_value());
}

TEST(ParserTests, DebugWithoutDir_Succeeds)
{
    std::vector<std::string> parts = {"prog", "--debug"};
    auto argv = make_argv(parts);

    auto res = CLI::parseArguments(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(res->debug);
    EXPECT_EQ(res->cwd, getExpectedHomeDirectory());
}

TEST(ParserTests, DebugAndVerboseWithoutDir_Succeeds)
{
    std::vector<std::string> parts = {"prog", "--debug", "--verbose"};
    auto argv = make_argv(parts);

    auto res = CLI::parseArguments(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(res->debug);
    EXPECT_TRUE(res->verbose);
    EXPECT_EQ(res->cwd, getExpectedHomeDirectory());
}

TEST(ParserTests, PositionalAndNamedDir_NamedWins)
{
    std::string pos = "/tmp";
    std::string named = "/var";
    std::vector<std::string> parts = {"prog", pos, "-d", named};
    auto argv = make_argv(parts);

    auto res = CLI::parseArguments(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->cwd, std::filesystem::path(named));
}

TEST(ParserTests, NamedDirTrailingSlashStripped)
{
    auto tempDir = std::filesystem::temp_directory_path();
    auto dirWithSlash = ensureTrailingSlash(tempDir);

    std::vector<std::string> parts = {"prog", "-d", dirWithSlash};
    auto argv = make_argv(parts);

    auto res = CLI::parseArguments(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->cwd, tempDir);
}

TEST(ParserTests, NamedRootDirRemainsRoot)
{
    auto root = std::filesystem::current_path().root_path();
    ASSERT_FALSE(root.empty());

    std::vector<std::string> parts = {"prog", "-d", root.string()};
    auto argv = make_argv(parts);

    auto res = CLI::parseArguments(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->cwd, root);
}

TEST(ParserTests, NamedFilePathResolvesToParent)
{
    auto tempFile = std::filesystem::temp_directory_path() / "cli_normalize_parent.txt";
    TempFileGuard guard(tempFile);

    std::vector<std::string> parts = {"prog", "-d", tempFile.string()};
    auto argv = make_argv(parts);

    auto res = CLI::parseArguments(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->cwd, tempFile.parent_path());
}

TEST(ParserTests, NamedRelativeFileResolvesToCurrentDirectory)
{
    auto cwd = std::filesystem::current_path();
    auto relativeFile = cwd / "cli_normalize_relative.txt";
    TempFileGuard guard(relativeFile);

    std::vector<std::string> parts = {"prog", "-d", relativeFile.filename().string()};
    auto argv = make_argv(parts);

    auto res = CLI::parseArguments(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->cwd, cwd);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
