#include <gtest/gtest.h>

#include <chrono>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "../src/core/FileOperationClipboard.hpp"
#include "../src/core/FileOperations.hpp"

namespace
{
    struct TempDirectory
    {
        std::filesystem::path path;

        TempDirectory()
            : path(std::filesystem::temp_directory_path() /
                   ("hyprfile_file_operation_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
        {
            std::filesystem::create_directories(path);
        }

        ~TempDirectory()
        {
            std::error_code ec;
            std::filesystem::permissions(path, std::filesystem::perms::owner_all,
                                         std::filesystem::perm_options::add, ec);
            std::filesystem::remove_all(path, ec);
        }
    };

    void writeText(const std::filesystem::path& path, const std::string& content)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file << content;
    }

    std::string readText(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    std::size_t entryCount(const std::filesystem::path& path)
    {
        return static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator(path),
                                                      std::filesystem::directory_iterator{}));
    }

    bool startsWith(const std::string& value, const std::string& prefix)
    {
        return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    }
}

TEST(FileOperationClipboardTests, ParsesMultipleNulSeparatedPathsIncludingNewlines)
{
    TempDirectory tempDir;
    const auto stateFile = tempDir.path / "clipboard-state";
    const auto firstPath = tempDir.path / "one with\nnewline.txt";
    const auto secondPath = tempDir.path / "two with spaces 'quotes' [brackets].txt";

    std::string raw = "copy\n";
    raw.append(firstPath.string());
    raw.push_back('\0');
    raw.append(secondPath.string());
    raw.push_back('\0');

    std::ofstream file(stateFile, std::ios::binary);
    file.write(raw.data(), static_cast<std::streamsize>(raw.size()));
    file.close();

    FileOperationClipboard clipboard(stateFile);
    const auto state = clipboard.read();

    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->operation, FileOperationClipboard::Operation::Copy);
    ASSERT_EQ(state->sources.size(), 2U);
    EXPECT_EQ(state->sources[0], firstPath);
    EXPECT_EQ(state->sources[1], secondPath);
}

TEST(FileOperationClipboardTests, WriteRestrictsStateDirectoryAndFilePermissions)
{
    TempDirectory tempDir;
    const auto stateFile = tempDir.path / "runtime" / "clipboard-state";
    const auto source = tempDir.path / "source.txt";
    writeText(source, "source");

    FileOperationClipboard clipboard(stateFile);

    ASSERT_TRUE(clipboard.write(FileOperationClipboard::Operation::Copy, {source}));

    const auto dirPerms = std::filesystem::status(stateFile.parent_path()).permissions();
    EXPECT_EQ(dirPerms & (std::filesystem::perms::group_all | std::filesystem::perms::others_all),
              std::filesystem::perms::none);

    const auto filePerms = std::filesystem::status(stateFile).permissions();
    EXPECT_EQ(filePerms & (std::filesystem::perms::group_all | std::filesystem::perms::others_all),
              std::filesystem::perms::none);
}

TEST(FileOperationClipboardTests, DefaultStateFileUsesHyprfileDirectoryName)
{
    const auto stateFile = FileOperationClipboard::defaultStateFile().string();

    EXPECT_NE(stateFile.find("hyprfile"), std::string::npos);
}

TEST(FileOperationClipboardTests, ClearConsumesStateWhenDirectoryUnlinkFailsButFileIsWritable)
{
    if (geteuid() == 0)
        GTEST_SKIP() << "Directory permission clear fallback is not meaningful as root";

    TempDirectory tempDir;
    const auto stateDir = tempDir.path / "runtime";
    const auto stateFile = stateDir / "clipboard-state";
    const auto source = tempDir.path / "source.txt";
    writeText(source, "source");

    FileOperationClipboard clipboard(stateFile);
    ASSERT_TRUE(clipboard.write(FileOperationClipboard::Operation::Copy, {source}));

    std::filesystem::permissions(stateDir, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::replace);

    const bool cleared = clipboard.clear();

    std::error_code cleanupEc;
    std::filesystem::permissions(stateDir, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, cleanupEc);

    EXPECT_TRUE(cleared);
    EXPECT_FALSE(clipboard.read().has_value());
}

TEST(FileOperationsTests, CopiesFileIntoAnotherDirectory)
{
    TempDirectory tempDir;
    const auto source = tempDir.path / "source" / "alpha.txt";
    const auto target = tempDir.path / "target";
    std::filesystem::create_directories(target);
    writeText(source, "alpha content");

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Copy, {source}}, target);

    ASSERT_TRUE(result.success) << result.error;
    ASSERT_EQ(result.destinations.size(), 1U);
    EXPECT_EQ(result.destinations[0], target / "alpha.txt");
    EXPECT_EQ(readText(target / "alpha.txt"), "alpha content");
    EXPECT_TRUE(std::filesystem::exists(source));
    EXPECT_TRUE(result.remainingSources.empty());
}

TEST(FileOperationsTests, CopiesDirectoryRecursivelyIntoAnotherDirectory)
{
    TempDirectory tempDir;
    const auto source = tempDir.path / "source" / "folder";
    const auto target = tempDir.path / "target";
    std::filesystem::create_directories(target);
    writeText(source / "nested" / "file.txt", "nested content");

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Copy, {source}}, target);

    ASSERT_TRUE(result.success) << result.error;
    ASSERT_EQ(result.destinations.size(), 1U);
    EXPECT_EQ(result.destinations[0], target / "folder");
    EXPECT_EQ(readText(target / "folder" / "nested" / "file.txt"), "nested content");
    EXPECT_TRUE(std::filesystem::exists(source / "nested" / "file.txt"));
}

TEST(FileOperationsTests, CutsFileIntoAnotherDirectory)
{
    TempDirectory tempDir;
    const auto source = tempDir.path / "source" / "move-me.txt";
    const auto target = tempDir.path / "target";
    std::filesystem::create_directories(target);
    writeText(source, "move content");

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Cut, {source}}, target);

    ASSERT_TRUE(result.success) << result.error;
    ASSERT_EQ(result.destinations.size(), 1U);
    EXPECT_EQ(result.destinations[0], target / "move-me.txt");
    EXPECT_FALSE(std::filesystem::exists(source));
    EXPECT_EQ(readText(target / "move-me.txt"), "move content");
}

TEST(FileOperationsTests, CutsDirectoryIntoAnotherDirectory)
{
    TempDirectory tempDir;
    const auto source = tempDir.path / "source" / "folder";
    const auto target = tempDir.path / "target";
    std::filesystem::create_directories(target);
    writeText(source / "child.txt", "child content");

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Cut, {source}}, target);

    ASSERT_TRUE(result.success) << result.error;
    ASSERT_EQ(result.destinations.size(), 1U);
    EXPECT_EQ(result.destinations[0], target / "folder");
    EXPECT_FALSE(std::filesystem::exists(source));
    EXPECT_EQ(readText(target / "folder" / "child.txt"), "child content");
}

TEST(FileOperationsTests, GeneratesCopyNamesForTargetConflicts)
{
    TempDirectory tempDir;
    const auto source = tempDir.path / "source" / "file.txt";
    const auto target = tempDir.path / "target";
    std::filesystem::create_directories(target);
    writeText(source, "new content");
    writeText(target / "file.txt", "existing content");
    writeText(target / "file copy.txt", "existing copy content");

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Copy, {source}}, target);

    ASSERT_TRUE(result.success) << result.error;
    ASSERT_EQ(result.destinations.size(), 1U);
    EXPECT_EQ(result.destinations[0], target / "file copy 2.txt");
    EXPECT_EQ(readText(target / "file copy 2.txt"), "new content");
    EXPECT_EQ(readText(target / "file.txt"), "existing content");
}

TEST(FileOperationsTests, BrokenDestinationSymlinkIsTreatedAsAConflict)
{
    TempDirectory tempDir;
    const auto source = tempDir.path / "source" / "file.txt";
    const auto target = tempDir.path / "target";
    std::filesystem::create_directories(target);
    writeText(source, "new content");

    std::error_code ec;
    std::filesystem::create_symlink(target / "missing-target.txt", target / "file.txt", ec);
    if (ec)
        GTEST_SKIP() << "symlinks are unavailable: " << ec.message();

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Copy, {source}}, target);

    ASSERT_TRUE(result.success) << result.error;
    ASSERT_EQ(result.destinations.size(), 1U);
    EXPECT_EQ(result.destinations[0], target / "file copy.txt");
    EXPECT_TRUE(std::filesystem::is_symlink(std::filesystem::symlink_status(target / "file.txt")));
    EXPECT_EQ(readText(target / "file copy.txt"), "new content");
    EXPECT_FALSE(std::filesystem::exists(target / "missing-target.txt"));
}

TEST(FileOperationsTests, PublishDoesNotReplaceDestinationCreatedAfterPreflight)
{
    TempDirectory tempDir;
    const auto source = tempDir.path / "source" / "folder";
    const auto target = tempDir.path / "target";
    std::filesystem::create_directories(target);
    for (int i = 0; i < 2000; ++i)
        writeText(source / ("file_" + std::to_string(i) + ".txt"), "content " + std::to_string(i));

    std::atomic<bool> stop = false;
    std::atomic<bool> createdRaceDestination = false;
    std::thread racer([&]()
                      {
                          const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                          while (!stop.load() && std::chrono::steady_clock::now() < deadline)
                          {
                              std::error_code ec;
                              for (const auto& entry : std::filesystem::directory_iterator(target, ec))
                              {
                                  if (startsWith(entry.path().filename().string(), ".folder.hyprfile-tmp-"))
                                  {
                                      std::filesystem::create_directory(target / "folder", ec);
                                      createdRaceDestination = true;
                                      return;
                                  }
                              }

                              std::this_thread::sleep_for(std::chrono::milliseconds(1));
                          }
                      });

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Copy, {source}}, target);
    stop = true;
    racer.join();

    if (!createdRaceDestination.load())
        GTEST_SKIP() << "race destination was not created before publish";

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(std::filesystem::is_directory(target / "folder"));
    EXPECT_EQ(entryCount(target / "folder"), 0U);
    EXPECT_TRUE(std::filesystem::exists(source / "file_0.txt"));
}

TEST(FileOperationsTests, CutIntoSameParentIsSuccessfulNoop)
{
    TempDirectory tempDir;
    const auto source = tempDir.path / "same-parent.txt";
    writeText(source, "stays put");

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Cut, {source}}, tempDir.path);

    ASSERT_TRUE(result.success) << result.error;
    ASSERT_EQ(result.destinations.size(), 1U);
    EXPECT_EQ(result.destinations[0], source);
    EXPECT_TRUE(std::filesystem::exists(source));
    EXPECT_EQ(readText(source), "stays put");
}

TEST(FileOperationsTests, MissingSourceFailsWithoutChangingTarget)
{
    TempDirectory tempDir;
    const auto source = tempDir.path / "missing.txt";
    const auto target = tempDir.path / "target";
    std::filesystem::create_directories(target);

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Copy, {source}}, target);

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.preflightFailed);
    EXPECT_TRUE(result.destinations.empty());
    ASSERT_EQ(result.remainingSources.size(), 1U);
    EXPECT_EQ(result.remainingSources[0], source);
    EXPECT_EQ(entryCount(target), 0U);
}

TEST(FileOperationsTests, PartialCopyFailureKeepsOnlyFailedAndUnattemptedSources)
{
    if (geteuid() == 0)
        GTEST_SKIP() << "Unreadable source partial failure check is not meaningful as root";

    TempDirectory tempDir;
    const auto firstSource = tempDir.path / "source" / "first.txt";
    const auto failedSource = tempDir.path / "source" / "unreadable.txt";
    const auto unattemptedSource = tempDir.path / "source" / "last.txt";
    const auto target = tempDir.path / "target";
    std::filesystem::create_directories(target);
    writeText(firstSource, "first");
    writeText(failedSource, "unreadable");
    writeText(unattemptedSource, "last");
    std::filesystem::permissions(failedSource, std::filesystem::perms::none, std::filesystem::perm_options::replace);

    const auto result = FileOperations::paste(
        {FileOperationClipboard::Operation::Copy, {firstSource, failedSource, unattemptedSource}}, target);

    std::error_code cleanupEc;
    std::filesystem::permissions(failedSource, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace, cleanupEc);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.preflightFailed);
    ASSERT_EQ(result.destinations.size(), 1U);
    EXPECT_EQ(result.destinations[0], target / "first.txt");
    ASSERT_EQ(result.remainingSources.size(), 2U);
    EXPECT_EQ(result.remainingSources[0], failedSource);
    EXPECT_EQ(result.remainingSources[1], unattemptedSource);
    EXPECT_TRUE(std::filesystem::exists(target / "first.txt"));
    EXPECT_FALSE(std::filesystem::exists(target / "unreadable.txt"));
    EXPECT_FALSE(std::filesystem::exists(target / "last.txt"));
}

TEST(FileOperationsTests, FailedFallbackCopyDoesNotDeleteOriginalSource)
{
    if (geteuid() == 0)
        GTEST_SKIP() << "Unreadable source fallback check is not meaningful as root";

    const std::filesystem::path shm = "/dev/shm";
    if (!std::filesystem::is_directory(shm))
        GTEST_SKIP() << "/dev/shm is unavailable";

    TempDirectory tempDir;
    const auto probe = tempDir.path / "cross-device-probe";
    const auto probeTarget = shm / ("hyprfile_probe_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    writeText(probe, "probe");
    std::error_code probeEc;
    std::filesystem::rename(probe, probeTarget, probeEc);
    if (!probeEc)
    {
        std::filesystem::remove(probeTarget);
        GTEST_SKIP() << "Temp directory and /dev/shm are not cross-device";
    }
    if (probeEc != std::errc::cross_device_link)
        GTEST_SKIP() << "Cross-device rename precondition failed with: " << probeEc.message();

    const auto source = tempDir.path / "unreadable.txt";
    const auto target = shm / ("hyprfile_file_operation_target_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(target);
    writeText(source, "secret");
    std::filesystem::permissions(source, std::filesystem::perms::none, std::filesystem::perm_options::replace);

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Cut, {source}}, target);

    std::error_code cleanupEc;
    std::filesystem::permissions(source, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace, cleanupEc);
    std::filesystem::remove_all(target, cleanupEc);

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(std::filesystem::exists(source));
}

TEST(FileOperationsTests, PathsWithSpacesAndSpecialCharactersArePreserved)
{
    TempDirectory tempDir;
    const auto source = tempDir.path / "source" / "name with spaces [x] 'quote'.txt";
    const auto target = tempDir.path / "target dir";
    std::filesystem::create_directories(target);
    writeText(source, "special content");

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Copy, {source}}, target);

    ASSERT_TRUE(result.success) << result.error;
    ASSERT_EQ(result.destinations.size(), 1U);
    EXPECT_EQ(result.destinations[0], target / "name with spaces [x] 'quote'.txt");
    EXPECT_EQ(readText(result.destinations[0]), "special content");
}

TEST(FileOperationsTests, DestinationUsesSymlinkTargetPathWhenPastingThroughSymlink)
{
    TempDirectory tempDir;
    const auto source = tempDir.path / "source" / "file.txt";
    const auto realTarget = tempDir.path / "real-target";
    const auto linkedTarget = tempDir.path / "linked-target";
    std::filesystem::create_directories(realTarget);
    writeText(source, "content");

    std::error_code ec;
    std::filesystem::create_directory_symlink(realTarget, linkedTarget, ec);
    if (ec)
        GTEST_SKIP() << "directory symlinks are unavailable: " << ec.message();

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Copy, {source}}, linkedTarget);

    ASSERT_TRUE(result.success) << result.error;
    ASSERT_EQ(result.destinations.size(), 1U);
    EXPECT_EQ(result.destinations[0], linkedTarget / "file.txt");
    EXPECT_EQ(readText(realTarget / "file.txt"), "content");
}

TEST(FileOperationsTests, BatchPasteHandlesMixedEntriesWithUniqueDestinations)
{
    TempDirectory tempDir;
    const auto firstFile = tempDir.path / "a" / "dup.txt";
    const auto secondFile = tempDir.path / "b" / "dup.txt";
    const auto sourceDir = tempDir.path / "a" / "folder";
    const auto target = tempDir.path / "target";
    std::filesystem::create_directories(target);
    writeText(firstFile, "first");
    writeText(secondFile, "second");
    writeText(sourceDir / "child.txt", "child");

    const auto result = FileOperations::paste({FileOperationClipboard::Operation::Copy, {firstFile, secondFile, sourceDir}}, target);

    ASSERT_TRUE(result.success) << result.error;
    ASSERT_EQ(result.destinations.size(), 3U);
    EXPECT_EQ(result.destinations[0], target / "dup.txt");
    EXPECT_EQ(result.destinations[1], target / "dup copy.txt");
    EXPECT_EQ(result.destinations[2], target / "folder");
    EXPECT_EQ(readText(target / "dup.txt"), "first");
    EXPECT_EQ(readText(target / "dup copy.txt"), "second");
    EXPECT_EQ(readText(target / "folder" / "child.txt"), "child");
}
