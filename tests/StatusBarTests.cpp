#include <gtest/gtest.h>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprutils/cli/Logger.hpp>

#include "../src/ui/StatusBar.hpp"

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
}

TEST(StatusBarTests, FormatItemCountTextHandlesZeroSingularAndPlural)
{
    EXPECT_EQ(formatItemCountText(0), "0 items");
    EXPECT_EQ(formatItemCountText(1), "1 item");
    EXPECT_EQ(formatItemCountText(42), "42 items");
}

TEST(StatusBarTests, DirectoryItemCountCanBeUpdated)
{
    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    StatusBar statusBar(backend.backend);

    EXPECT_NO_FATAL_FAILURE(statusBar.setDirectoryItemCount(3));
}
