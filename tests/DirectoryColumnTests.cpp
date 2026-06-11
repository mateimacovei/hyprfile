#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprutils/cli/Logger.hpp>
#include <hyprutils/math/Box.hpp>

#include "../src/core/FileOperationClipboard.hpp"
#include "../src/ui/columns/layout/DirectoryColumn.hpp"
#include "../src/ui/model/FileItemLayout.hpp"

static_assert(std::is_same_v<decltype(std::declval<DirectoryColumn &>().openTerminal()), void>);
static_assert(std::is_same_v<decltype(std::declval<DirectoryColumn &>().pasteSelection()), DirectoryColumn::PasteSelectionResult>);

namespace
{
    struct BackendGuard
    {
        BackendGuard()
        {
            logger = Hyprutils::Memory::makeShared<Hyprutils::CLI::CLogger>();
            logger->setEnableStdout(false);
            logger->setLogLevel(Hyprutils::CLI::LOG_WARN);

            Hyprtoolkit::IBackend::SBackendCreationData backendData;
            loggerConn = Hyprutils::Memory::makeShared<Hyprutils::CLI::CLoggerConnection>(*logger);
            loggerConn->setLogLevel(Hyprutils::CLI::LOG_WARN);
            backendData.pLogConnection = loggerConn;

            backend = Hyprtoolkit::IBackend::createWithData(backendData);
        }

        ~BackendGuard()
        {
            if (backend)
                backend->destroy();
        }

        SP<Hyprutils::CLI::CLogger> logger;
        SP<Hyprutils::CLI::CLoggerConnection> loggerConn;
        SP<Hyprtoolkit::IBackend> backend;
    };

    struct TempDirectory
    {
        std::filesystem::path path;

        TempDirectory()
            : path(std::filesystem::temp_directory_path() /
                   ("hyprfile_directory_column_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
        {
            std::filesystem::create_directory(path);
        }

        ~TempDirectory()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    };

    struct HiddenFilesGuard
    {
        explicit HiddenFilesGuard(bool visible)
            : previous(FileSystemService::get().showHiddenFiles())
        {
            FileSystemService::get().setShowHiddenFiles(visible);
        }

        ~HiddenFilesGuard()
        {
            FileSystemService::get().setShowHiddenFiles(previous);
        }

        bool previous;
    };

    struct RuntimeDirGuard
    {
        explicit RuntimeDirGuard(const std::filesystem::path &runtimeDir)
        {
            if (const char *value = std::getenv("XDG_RUNTIME_DIR"); value && *value != '\0')
            {
                hadPrevious = true;
                previous = value;
            }

            std::filesystem::create_directories(runtimeDir);
            std::filesystem::permissions(runtimeDir, std::filesystem::perms::owner_all,
                                         std::filesystem::perm_options::replace);
            setenv("XDG_RUNTIME_DIR", runtimeDir.c_str(), 1);
            FileOperationClipboard().clear();
        }

        ~RuntimeDirGuard()
        {
            FileOperationClipboard().clear();
            if (hadPrevious)
                setenv("XDG_RUNTIME_DIR", previous.c_str(), 1);
            else
                unsetenv("XDG_RUNTIME_DIR");
        }

        bool hadPrevious = false;
        std::string previous;
    };

    struct PathGuard
    {
        explicit PathGuard(const std::filesystem::path &prependPath)
        {
            if (const char *value = std::getenv("PATH"); value && *value != '\0')
            {
                hadPrevious = true;
                previous = value;
            }

            const auto newPath = prependPath.string() + (hadPrevious ? ":" + previous : "");
            setenv("PATH", newPath.c_str(), 1);
        }

        ~PathGuard()
        {
            if (hadPrevious)
                setenv("PATH", previous.c_str(), 1);
            else
                unsetenv("PATH");
        }

        bool hadPrevious = false;
        std::string previous;
    };

    void createFakeGioTrash(const std::filesystem::path &binDir)
    {
        std::filesystem::create_directories(binDir);
        const auto gio = binDir / "gio";
        std::ofstream script(gio);
        script << "#!/bin/sh\n"
               << "if [ \"$1\" != \"trash\" ]; then exit 2; fi\n"
               << "rm -rf -- \"$2\"\n";
        script.close();
        std::filesystem::permissions(gio, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
    }

    void createFakeGioTrashFailingFor(const std::filesystem::path &binDir, const std::string &failingName)
    {
        std::filesystem::create_directories(binDir);
        const auto gio = binDir / "gio";
        std::ofstream script(gio);
        script << "#!/bin/sh\n"
               << "if [ \"$1\" != \"trash\" ]; then exit 2; fi\n"
               << "if [ \"$(basename \"$2\")\" = \"" << failingName << "\" ]; then exit 1; fi\n"
               << "rm -rf -- \"$2\"\n";
        script.close();
        std::filesystem::permissions(gio, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
    }

    void createFiles(const std::filesystem::path &dir, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            std::ofstream file(dir / ("file_" + std::to_string(i) + ".txt"));
            file << "test";
        }
    }

    void createNamedFiles(const std::filesystem::path &dir, const std::vector<std::string> &names)
    {
        for (const auto &name : names)
        {
            std::ofstream file(dir / name);
            file << "test";
        }
    }

    std::string selectedFilename(DirectoryColumn &column)
    {
        auto selected = column.getSelection().lock();
        if (!selected)
            return {};
        return selected->getPath().filename().string();
    }

    std::vector<std::string> clipboardSourceFilenames()
    {
        auto state = FileOperationClipboard().read();
        if (!state)
            return {};

        std::vector<std::string> names;
        names.reserve(state->sources.size());
        for (const auto &source : state->sources)
            names.push_back(source.filename().string());
        return names;
    }

    void resizeColumn(DirectoryColumn &column, float height)
    {
        SP<Hyprtoolkit::IElement> layout = column.getLayout();
        layout->reposition(Hyprutils::Math::CBox(0, 0, 400, height));
    }
}

TEST(DirectoryColumnTests, ResizingLargerNearBottomDoesNotReadPastEntries)
{
    TempDirectory tempDir;
    createFiles(tempDir.path, 120);

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();

    resizeColumn(column, 60.F);
    auto selected = column.setSelection(119, false).lock();
    ASSERT_TRUE(selected);

    resizeColumn(column, 2000.F);

    ASSERT_NO_FATAL_FAILURE(column.resync());
    auto afterResize = column.getSelection().lock();
    ASSERT_TRUE(afterResize);
    EXPECT_EQ(afterResize->getPath(), selected->getPath());
}

TEST(DirectoryColumnTests, ToggleHiddenFilesShowsDotfilesAndPreservesSelection)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;

    std::ofstream visible(tempDir.path / "visible.txt");
    visible << "test";
    std::ofstream hidden(tempDir.path / ".hidden.txt");
    hidden << "test";

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();

    EXPECT_EQ(column.getTotalItemsCount(), 1);
    auto initialSelection = column.getSelection().lock();
    ASSERT_TRUE(initialSelection);
    EXPECT_EQ(initialSelection->getPath().filename().string(), "visible.txt");

    auto shownSelection = column.toggleHidden().lock();
    ASSERT_TRUE(shownSelection);
    EXPECT_TRUE(FileSystemService::get().showHiddenFiles());
    EXPECT_EQ(column.getTotalItemsCount(), 2);
    EXPECT_EQ(shownSelection->getPath().filename().string(), "visible.txt");

    auto hiddenSelection = column.toggleHidden().lock();
    ASSERT_TRUE(hiddenSelection);
    EXPECT_FALSE(FileSystemService::get().showHiddenFiles());
    EXPECT_EQ(column.getTotalItemsCount(), 1);
    EXPECT_EQ(hiddenSelection->getPath().filename().string(), "visible.txt");
}

TEST(DirectoryColumnTests, CommitSearchJumpsToFirstCaseInsensitiveMatchAfterSelection)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "delta.txt", "omega.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());

    column.setSearchQuery("TA");
    auto result = column.commitSearch().lock();

    ASSERT_TRUE(result);
    EXPECT_EQ(result->getPath().filename().string(), "delta.txt");
    EXPECT_EQ(selectedFilename(column), "delta.txt");
}

TEST(DirectoryColumnTests, SearchNextAndPreviousWrapWithCounts)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "match-one.txt", "middle.txt", "match-two.txt", "zzz-match.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    column.setSearchQuery("match");
    ASSERT_TRUE(column.setSelection(tempDir.path / "match-one.txt").lock());

    auto next = column.jumpToNextSearchResult(2).lock();
    ASSERT_TRUE(next);
    EXPECT_EQ(next->getPath().filename().string(), "zzz-match.txt");

    auto previous = column.jumpToPreviousSearchResult(3).lock();
    ASSERT_TRUE(previous);
    EXPECT_EQ(previous->getPath().filename().string(), "zzz-match.txt");
    EXPECT_EQ(selectedFilename(column), "zzz-match.txt");
}

TEST(DirectoryColumnTests, SearchNoMatchPreservesSelection)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());

    column.setSearchQuery("missing");
    auto result = column.commitSearch().lock();

    ASSERT_TRUE(result);
    EXPECT_EQ(result->getPath().filename().string(), "bravo.txt");
    EXPECT_EQ(selectedFilename(column), "bravo.txt");
}

TEST(DirectoryColumnTests, ClearingSearchDisablesSearchNavigation)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "match-one.txt", "match-two.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    column.setSearchQuery("match");
    ASSERT_TRUE(column.commitSearch().lock());

    column.clearSearch();
    ASSERT_TRUE(column.setSelection(tempDir.path / "alpha.txt").lock());
    auto result = column.jumpToNextSearchResult(1).lock();

    ASSERT_TRUE(result);
    EXPECT_EQ(result->getPath().filename().string(), "alpha.txt");
    EXPECT_EQ(selectedFilename(column), "alpha.txt");
}

TEST(DirectoryColumnTests, NextSearchWithoutActiveSearchLogsAndPreservesSelection)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());

    testing::internal::CaptureStderr();
    auto result = column.jumpToNextSearchResult(1).lock();
    const std::string logs = testing::internal::GetCapturedStderr();

    ASSERT_TRUE(result);
    EXPECT_EQ(result->getPath().filename().string(), "bravo.txt");
    EXPECT_NE(logs.find("search is not active"), std::string::npos);
}

TEST(DirectoryColumnTests, PreviousSearchWithoutActiveSearchLogsAndPreservesSelection)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());

    testing::internal::CaptureStderr();
    auto result = column.jumpToPreviousSearchResult(1).lock();
    const std::string logs = testing::internal::GetCapturedStderr();

    ASSERT_TRUE(result);
    EXPECT_EQ(result->getPath().filename().string(), "bravo.txt");
    EXPECT_NE(logs.find("search is not active"), std::string::npos);
}

TEST(DirectoryColumnTests, PasteWithoutClipboardDoesNotRequestPreviewRefresh)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);
    RuntimeDirGuard runtimeDir(tempDir.path / "runtime");

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());

    const auto result = column.pasteSelection();

    EXPECT_FALSE(result.refreshedDirectory);
    auto selection = result.selection.lock();
    ASSERT_TRUE(selection);
    EXPECT_EQ(selection->getPath().filename().string(), "bravo.txt");
    EXPECT_EQ(selectedFilename(column), "bravo.txt");
}

TEST(DirectoryColumnTests, CutPasteIntoSameParentConsumesClipboardWithoutPreviewRefresh)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"same-parent.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);
    RuntimeDirGuard runtimeDir(tempDir.path / "runtime");
    ASSERT_TRUE(FileOperationClipboard().write(FileOperationClipboard::Operation::Cut,
                                              {tempDir.path / "same-parent.txt"}));

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "same-parent.txt").lock());

    const auto result = column.pasteSelection();

    EXPECT_FALSE(result.refreshedDirectory);
    auto selection = result.selection.lock();
    ASSERT_TRUE(selection);
    EXPECT_EQ(selection->getPath(), tempDir.path / "same-parent.txt");
    EXPECT_FALSE(FileOperationClipboard().read().has_value());
}

TEST(DirectoryColumnTests, SuccessfulPasteRequestsPreviewRefreshAndSelectsDestination)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    const auto sourceDir = tempDir.path / "source";
    const auto targetDir = tempDir.path / "target";
    std::filesystem::create_directories(sourceDir);
    std::filesystem::create_directories(targetDir);
    createNamedFiles(sourceDir, {"copied.txt"});
    createNamedFiles(targetDir, {"alpha.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);
    RuntimeDirGuard runtimeDir(tempDir.path / "runtime");
    ASSERT_TRUE(FileOperationClipboard().write(FileOperationClipboard::Operation::Copy,
                                              {sourceDir / "copied.txt"}));

    DirectoryColumn column(backend.backend, false, targetDir, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(targetDir / "alpha.txt").lock());

    const auto result = column.pasteSelection();

    EXPECT_TRUE(result.refreshedDirectory);
    auto selection = result.selection.lock();
    ASSERT_TRUE(selection);
    EXPECT_EQ(selection->getPath(), targetDir / "copied.txt");
    EXPECT_EQ(selectedFilename(column), "copied.txt");
}

TEST(DirectoryColumnTests, StartSelectionCopiesOnlyCursorWhenCursorDoesNotMove)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);
    RuntimeDirGuard runtimeDir(tempDir.path / "runtime");

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());

    auto started = column.startSelection().lock();

    ASSERT_TRUE(started);
    EXPECT_TRUE(column.hasMultiSelection());
    EXPECT_EQ(selectedFilename(column), "bravo.txt");

    ASSERT_TRUE(column.copySelection().lock());
    auto state = FileOperationClipboard().read();
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->operation, FileOperationClipboard::Operation::Copy);
    EXPECT_EQ(clipboardSourceFilenames(), (std::vector<std::string>{"bravo.txt"}));
    EXPECT_FALSE(column.hasMultiSelection());
}

TEST(DirectoryColumnTests, MultiSelectionCopyExpandsDownInclusivelyAndCancels)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt", "delta.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);
    RuntimeDirGuard runtimeDir(tempDir.path / "runtime");

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());
    ASSERT_TRUE(column.startSelection().lock());
    ASSERT_TRUE(column.moveDown(2, false).lock());

    ASSERT_TRUE(column.copySelection().lock());

    auto state = FileOperationClipboard().read();
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->operation, FileOperationClipboard::Operation::Copy);
    EXPECT_EQ(clipboardSourceFilenames(), (std::vector<std::string>{"bravo.txt", "charlie.txt", "delta.txt"}));
    EXPECT_FALSE(column.hasMultiSelection());
}

TEST(DirectoryColumnTests, CurrentItemMultiSelectionIndicatorUsesVisibleMarkerColor)
{
    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    const auto color = fileItemMultiSelectionIndicatorColor(backend.backend, true, true);

    EXPECT_EQ(color, backend.backend->getPalette()->m_colors.text);
    EXPECT_NE(color, backend.backend->getPalette()->m_colors.background);
}

TEST(DirectoryColumnTests, MultiSelectionCutExpandsUpInclusivelyAndCancels)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt", "delta.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);
    RuntimeDirGuard runtimeDir(tempDir.path / "runtime");

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "delta.txt").lock());
    ASSERT_TRUE(column.startSelection().lock());
    ASSERT_TRUE(column.moveUp(2, false).lock());

    ASSERT_TRUE(column.cutSelection().lock());

    auto state = FileOperationClipboard().read();
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->operation, FileOperationClipboard::Operation::Cut);
    EXPECT_EQ(clipboardSourceFilenames(), (std::vector<std::string>{"bravo.txt", "charlie.txt", "delta.txt"}));
    EXPECT_FALSE(column.hasMultiSelection());
}

TEST(DirectoryColumnTests, StartSelectionAgainRestartsSourceAtCursor)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt", "delta.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);
    RuntimeDirGuard runtimeDir(tempDir.path / "runtime");

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "alpha.txt").lock());
    ASSERT_TRUE(column.startSelection().lock());
    ASSERT_TRUE(column.moveDown(2, false).lock());

    ASSERT_TRUE(column.startSelection().lock());
    ASSERT_TRUE(column.copySelection().lock());

    EXPECT_EQ(clipboardSourceFilenames(), (std::vector<std::string>{"charlie.txt"}));
    EXPECT_FALSE(column.hasMultiSelection());
}

TEST(DirectoryColumnTests, RefreshPreservesMultiSelectionSourcePath)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);
    RuntimeDirGuard runtimeDir(tempDir.path / "runtime");

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());
    ASSERT_TRUE(column.startSelection().lock());
    createNamedFiles(tempDir.path, {"aardvark.txt"});

    ASSERT_TRUE(column.refresh().lock());
    EXPECT_TRUE(column.hasMultiSelection());
    ASSERT_TRUE(column.moveUp(1, false).lock());
    ASSERT_TRUE(column.copySelection().lock());

    EXPECT_EQ(clipboardSourceFilenames(), (std::vector<std::string>{"alpha.txt", "bravo.txt"}));
    EXPECT_FALSE(column.hasMultiSelection());
}

TEST(DirectoryColumnTests, RefreshCancelsMultiSelectionWhenSourceDisappears)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());
    ASSERT_TRUE(column.startSelection().lock());
    std::filesystem::remove(tempDir.path / "bravo.txt");

    ASSERT_TRUE(column.refresh().lock());

    EXPECT_FALSE(column.hasMultiSelection());
}

TEST(DirectoryColumnTests, MultiSelectionCopyIncludesSkippedPageItems)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt", "delta.txt", "echo.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);
    RuntimeDirGuard runtimeDir(tempDir.path / "runtime");

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    resizeColumn(column, 40.F);
    ASSERT_TRUE(column.setSelection(tempDir.path / "alpha.txt").lock());
    ASSERT_TRUE(column.startSelection().lock());

    ASSERT_TRUE(column.pageDown(false).lock());
    ASSERT_TRUE(column.copySelection().lock());

    EXPECT_EQ(clipboardSourceFilenames(), (std::vector<std::string>{"alpha.txt", "bravo.txt", "charlie.txt"}));
    EXPECT_FALSE(column.hasMultiSelection());
}

TEST(DirectoryColumnTests, MultiSelectionCopyIncludesSkippedSearchJumpItems)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt", "delta-match.txt", "echo.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);
    RuntimeDirGuard runtimeDir(tempDir.path / "runtime");

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "alpha.txt").lock());
    ASSERT_TRUE(column.startSelection().lock());
    column.setSearchQuery("match");

    ASSERT_TRUE(column.jumpToNextSearchResult().lock());
    ASSERT_TRUE(column.copySelection().lock());

    EXPECT_EQ(clipboardSourceFilenames(), (std::vector<std::string>{"alpha.txt", "bravo.txt", "charlie.txt", "delta-match.txt"}));
    EXPECT_FALSE(column.hasMultiSelection());
}

TEST(DirectoryColumnTests, InactiveSelectionCopyAndCutWriteOnlyCursor)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt"});

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);
    RuntimeDirGuard runtimeDir(tempDir.path / "runtime");

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());

    ASSERT_TRUE(column.copySelection().lock());
    auto copyState = FileOperationClipboard().read();
    ASSERT_TRUE(copyState.has_value());
    EXPECT_EQ(copyState->operation, FileOperationClipboard::Operation::Copy);
    EXPECT_EQ(clipboardSourceFilenames(), (std::vector<std::string>{"bravo.txt"}));
    EXPECT_FALSE(column.hasMultiSelection());

    ASSERT_TRUE(column.cutSelection().lock());
    auto cutState = FileOperationClipboard().read();
    ASSERT_TRUE(cutState.has_value());
    EXPECT_EQ(cutState->operation, FileOperationClipboard::Operation::Cut);
    EXPECT_EQ(clipboardSourceFilenames(), (std::vector<std::string>{"bravo.txt"}));
    EXPECT_FALSE(column.hasMultiSelection());
}

TEST(DirectoryColumnTests, MultiSelectionTrashDeletesRangeSelectsFollowingItemAndCancels)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt", "delta.txt"});
    createFakeGioTrash(tempDir.path / "bin");
    PathGuard pathGuard(tempDir.path / "bin");

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());
    ASSERT_TRUE(column.startSelection().lock());
    ASSERT_TRUE(column.moveDown(1, false).lock());

    auto selection = column.trash().lock();

    ASSERT_TRUE(selection);
    EXPECT_EQ(selection->getPath().filename().string(), "delta.txt");
    EXPECT_EQ(selectedFilename(column), "delta.txt");
    EXPECT_FALSE(std::filesystem::exists(tempDir.path / "bravo.txt"));
    EXPECT_FALSE(std::filesystem::exists(tempDir.path / "charlie.txt"));
    EXPECT_TRUE(std::filesystem::exists(tempDir.path / "alpha.txt"));
    EXPECT_TRUE(std::filesystem::exists(tempDir.path / "delta.txt"));
    EXPECT_FALSE(column.hasMultiSelection());
}

TEST(DirectoryColumnTests, InactiveSelectionTrashDeletesOnlyCursorAndSelectsFollowingItem)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt"});
    createFakeGioTrash(tempDir.path / "bin");
    PathGuard pathGuard(tempDir.path / "bin");

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());

    auto selection = column.trash().lock();

    ASSERT_TRUE(selection);
    EXPECT_EQ(selection->getPath().filename().string(), "charlie.txt");
    EXPECT_FALSE(std::filesystem::exists(tempDir.path / "bravo.txt"));
    EXPECT_TRUE(std::filesystem::exists(tempDir.path / "alpha.txt"));
    EXPECT_TRUE(std::filesystem::exists(tempDir.path / "charlie.txt"));
    EXPECT_FALSE(column.hasMultiSelection());
}

TEST(DirectoryColumnTests, MultiSelectionTrashFailureStopsRefreshesAndCancels)
{
    HiddenFilesGuard hiddenFiles(false);
    TempDirectory tempDir;
    createNamedFiles(tempDir.path, {"alpha.txt", "bravo.txt", "charlie.txt", "delta.txt", "echo.txt"});
    createFakeGioTrashFailingFor(tempDir.path / "bin", "charlie.txt");
    PathGuard pathGuard(tempDir.path / "bin");

    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    DirectoryColumn column(backend.backend, false, tempDir.path, 1.F);
    column.draw();
    ASSERT_TRUE(column.setSelection(tempDir.path / "bravo.txt").lock());
    ASSERT_TRUE(column.startSelection().lock());
    ASSERT_TRUE(column.moveDown(2, false).lock());

    testing::internal::CaptureStderr();
    auto selection = column.trash().lock();
    const std::string logs = testing::internal::GetCapturedStderr();

    ASSERT_TRUE(selection);
    EXPECT_EQ(selection->getPath().filename().string(), "delta.txt");
    EXPECT_FALSE(std::filesystem::exists(tempDir.path / "bravo.txt"));
    EXPECT_TRUE(std::filesystem::exists(tempDir.path / "charlie.txt"));
    EXPECT_TRUE(std::filesystem::exists(tempDir.path / "delta.txt"));
    EXPECT_TRUE(std::filesystem::exists(tempDir.path / "echo.txt"));
    EXPECT_FALSE(column.hasMultiSelection());
    EXPECT_NE(logs.find("trashWithGio failed"), std::string::npos);
}
