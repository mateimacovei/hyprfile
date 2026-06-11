#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/Element.hpp>
#include <hyprtoolkit/types/SizeType.hpp>
#include <hyprutils/cli/Logger.hpp>
#include <hyprutils/math/Box.hpp>

#include "../src/ui/columns/layout/PreviewTextColumn.cpp"

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

        SP<Hyprutils::CLI::CLogger> logger;
        SP<Hyprutils::CLI::CLoggerConnection> loggerConn;
        SP<Hyprtoolkit::IBackend> backend;
    };

    struct TempFile
    {
        std::filesystem::path path;

        TempFile()
            : path(std::filesystem::temp_directory_path() /
                   ("hyprfile_preview_text_test_" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt"))
        {
        }

        ~TempFile()
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    };
}

TEST(PreviewTextColumnTests, ReadPreviewLinesStopsAtDisplayLineLimit)
{
    TempFile tempFile;
    const std::string longLine(64 * 1024, 'x');

    std::ofstream file(tempFile.path, std::ios::binary);
    file << longLine << "\nsecond line\n";
    file.close();

    const auto preview = readPreviewLines(tempFile.path, 10, 1024 * 1024);

    ASSERT_TRUE(preview.success);
    ASSERT_FALSE(preview.lines.empty());
    EXPECT_LE(preview.lines.front().size(), 512U);
    EXPECT_TRUE(preview.lines.front().ends_with("..."));
    EXPECT_TRUE(preview.truncated);
}

TEST(PreviewTextColumnTests, ReadPreviewLinesStopsAtVisibleRowLimit)
{
    TempFile tempFile;

    std::ofstream file(tempFile.path, std::ios::binary);
    for (int i = 0; i < 20; ++i)
        file << "line " << i << "\n";
    file.close();

    const auto preview = readPreviewLines(tempFile.path, 3, 1024 * 1024, 120);

    ASSERT_TRUE(preview.success);
    ASSERT_EQ(preview.lines.size(), 3U);
    EXPECT_EQ(preview.lines[0], "line 0");
    EXPECT_EQ(preview.lines[1], "line 1");
    EXPECT_EQ(preview.lines[2], "line 2");
    EXPECT_TRUE(preview.truncated);
}

TEST(PreviewTextColumnTests, PreviewLineRowUsesFullWidthContainer)
{
    BackendGuard backend;
    ASSERT_TRUE(backend.backend);

    auto column = CColumnLayoutBuilder::begin()
                      ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                      ->commence();
    auto row = makePreviewLineRow("abc", Hyprtoolkit::CFontSize{Hyprtoolkit::CFontSize::HT_FONT_TEXT},
                                  []() -> Hyprtoolkit::CHyprColor
                                  { return Hyprtoolkit::CHyprColor(1.0, 1.0, 1.0, 1.0); });

    column->addChild(row);
    SP<IElement> columnElement = column;
    columnElement->reposition(Hyprutils::Math::CBox(0, 0, 300, 100));

    EXPECT_FLOAT_EQ(row->posFromParent().x, 0.F);
    EXPECT_FLOAT_EQ(row->size().x, 300.F);
}
