#include <gtest/gtest.h>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/element/Element.hpp>
#include <hyprutils/cli/Logger.hpp>
#include <hyprutils/math/Box.hpp>

#include "core/MountInfoService.hpp"
#include "ui/columns/layout/MountInfoColumn.hpp"

namespace
{
    struct BackendGuard
    {
        BackendGuard()
        {
            logger = Hyprutils::Memory::makeShared<Hyprutils::CLI::CLogger>();
            logger->setEnableStdout(false);
            logger->setLogLevel(Hyprutils::CLI::LOG_WARN);

            loggerConn = Hyprutils::Memory::makeShared<Hyprutils::CLI::CLoggerConnection>(*logger);
            loggerConn->setLogLevel(Hyprutils::CLI::LOG_WARN);

            Hyprtoolkit::IBackend::SBackendCreationData backendData;
            backendData.pLogConnection = loggerConn;

            backend = Hyprtoolkit::IBackend::createWithData(backendData);
        }

        ~BackendGuard()
        {
            if (backend)
                backend->destroy();
        }

        Hyprutils::Memory::CSharedPointer<Hyprutils::CLI::CLogger> logger;
        Hyprutils::Memory::CSharedPointer<Hyprutils::CLI::CLoggerConnection> loggerConn;
        Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> backend;
    };

    void expectFullWidthRow(const Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CRowLayoutElement>& row)
    {
        EXPECT_GT(row->size().x, 250.F);
        EXPECT_LE(row->posFromParent().x, 1.F);
    }
}

TEST(MountInfoColumnTests, MountEntryTextRowsFillWidthAndEntriesHaveSeparators)
{
    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    const std::vector<MountInfo> mounts{
        MountInfo{"System", 1024ULL * 1024ULL * 1024ULL, 512ULL * 1024ULL * 1024ULL, {"/"}},
        MountInfo{"Archive", 2ULL * 1024ULL * 1024ULL * 1024ULL, 1024ULL * 1024ULL * 1024ULL, {"/mnt/archive"}},
    };

    const auto layout = hyprfile::UI::makeMountInfoColumnContent(backend.backend, mounts);
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement> root = layout.root;

    root->reposition(Hyprutils::Math::CBox(0, 0, 320, 240));

    ASSERT_EQ(layout.textRows.size(), 6);
    ASSERT_EQ(layout.separators.size(), 1);
    for (const auto& row : layout.textRows)
        expectFullWidthRow(row);

    EXPECT_GT(layout.separators[0]->size().x, 250.F);
    EXPECT_EQ(layout.separators[0]->size().y, 1.F);
}

TEST(MountInfoColumnTests, EmptyStateTextRowFillsWidth)
{
    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    const auto layout = hyprfile::UI::makeMountInfoColumnContent(backend.backend, {});
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement> root = layout.root;

    root->reposition(Hyprutils::Math::CBox(0, 0, 320, 240));

    ASSERT_EQ(layout.textRows.size(), 1);
    EXPECT_TRUE(layout.separators.empty());
    expectFullWidthRow(layout.textRows[0]);
}
