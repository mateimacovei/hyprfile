#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprutils/cli/Logger.hpp>

#include "../src/ui/columns/ParentColumn.hpp"

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
                   ("hyprfile_parent_column_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
        {
            std::filesystem::create_directory(path);
        }

        ~TempDirectory()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    };
}

TEST(ParentColumnTests, RootUsesReadOnlyMountInfoLayout)
{
    BackendGuard guard;
    ParentColumn parent(guard.backend, 0.16F);
    std::filesystem::path root("/");

    auto layout = parent.instantiateParentColumn(root);
    parent.set_layout_column(layout);

    EXPECT_TRUE(parent.get_layout_column());
    EXPECT_FALSE(parent.get_layout_directory_column());
    EXPECT_FALSE(parent.setSelection(root));
}

TEST(ParentColumnTests, NonRootUsesDirectoryLayout)
{
    BackendGuard guard;
    TempDirectory temp;
    ParentColumn parent(guard.backend, 0.16F);

    auto layout = parent.instantiateParentColumn(temp.path);
    parent.set_layout_column(layout);

    EXPECT_TRUE(parent.get_layout_column());
    EXPECT_TRUE(parent.get_layout_directory_column());
    EXPECT_EQ(parent.get_layout_column()->getPath(), temp.path.parent_path());
}
