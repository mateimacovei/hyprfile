#include "PreviewTextColumn.hpp"

#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/ScrollArea.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr std::size_t kMaxPreviewBytes = 1 * 1024 * 1024;
    constexpr int kFallbackVisibleRows = 12;
    constexpr std::size_t kFallbackVisibleColumns = 120;
    constexpr float kAverageGlyphWidthRatio = 0.6F;
    constexpr float kLineSpacing = 4.F;

    float estimateLineHeight(const CSharedPointer<IBackend> &backend)
    {
        const float baseSize = static_cast<float>(backend->getPalette()->m_vars.fontSize);
        return std::max(1.F, baseSize + kLineSpacing);
    }

    int computeVisibleRows(const CSharedPointer<CColumnLayoutElement> &layout, const CSharedPointer<IBackend> &backend)
    {
        const float availableHeight = layout->size().y;
        if (availableHeight <= 0.F)
            return kFallbackVisibleRows;

        const float rowHeight = estimateLineHeight(backend);
        const int estimatedRows = static_cast<int>(std::floor(availableHeight / rowHeight));
        return std::max(1, estimatedRows);
    }

    std::size_t computeVisibleColumns(const CSharedPointer<CColumnLayoutElement> &layout, const CSharedPointer<IBackend> &backend)
    {
        const float availableWidth = layout->size().x;
        if (availableWidth <= 0.F)
            return kFallbackVisibleColumns;

        const float baseSize = static_cast<float>(backend->getPalette()->m_vars.fontSize);
        const float estimatedGlyphWidth = std::max(1.F, baseSize * kAverageGlyphWidthRatio);
        const auto estimatedColumns = static_cast<std::size_t>(std::floor(availableWidth / estimatedGlyphWidth));
        return std::max<std::size_t>(1, estimatedColumns);
    }

    void markLineTruncated(std::string &line, std::size_t maxLineBytes)
    {
        if (maxLineBytes > 3)
        {
            line.resize(maxLineBytes - 3);
            line += "...";
            return;
        }

        line.assign(maxLineBytes, '.');
    }

    SP<CRowLayoutElement> makePreviewLineRow(std::string text, Hyprtoolkit::CFontSize fontSize, Hyprtoolkit::colorFn color)
    {
        auto row = CRowLayoutBuilder::begin()
                       ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                       ->commence();

        row->addChild(CTextBuilder::begin()
                          ->text(std::move(text))
                          ->fontSize(std::move(fontSize))
                          ->align(Hyprtoolkit::HT_FONT_ALIGN_LEFT)
                          ->color(std::move(color))
                          ->commence());

        return row;
    }

    struct PreviewReadResult
    {
        std::vector<std::string> lines;
        bool truncated = false;
        bool success = true;
    };

    PreviewReadResult readPreviewLines(const std::filesystem::path &path, std::size_t maxLines, std::size_t maxBytes,
                                       std::size_t maxLineBytes = kFallbackVisibleColumns)
    {
        PreviewReadResult result;
        result.lines.reserve(maxLines);
        const std::size_t lineLimit = std::max<std::size_t>(1, maxLineBytes);

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            result.success = false;
            return result;
        }

        std::size_t bytesRead = 0;
        std::string currentLine;
        currentLine.reserve(256);

        while (result.lines.size() < maxLines && bytesRead < maxBytes)
        {
            int nextChar = file.get();
            if (nextChar == EOF)
                break;

            ++bytesRead;
            if (nextChar == '\r')
                continue;

            if (nextChar == '\n')
            {
                result.lines.push_back(std::move(currentLine));
                currentLine.clear();
                continue;
            }

            currentLine.push_back(static_cast<char>(nextChar));
            if (currentLine.size() >= lineLimit)
            {
                const int followingChar = file.peek();
                if (followingChar != EOF && followingChar != '\n' && followingChar != '\r')
                {
                    markLineTruncated(currentLine, lineLimit);
                    result.lines.push_back(std::move(currentLine));
                    result.truncated = true;
                    return result;
                }
            }
        }

        if (!currentLine.empty() && result.lines.size() < maxLines)
            result.lines.push_back(std::move(currentLine));

        if (bytesRead >= maxBytes)
        {
            result.truncated = true;
        }
        else if (result.lines.size() >= maxLines && file && file.peek() != EOF)
        {
            result.truncated = true;
        }

        return result;
    }
}

PreviewTextColumn::PreviewTextColumn(CSharedPointer<IBackend> backend, std::filesystem::path path, float widthPercent)
    : BaseLayoutColumn(backend, std::move(path), widthPercent)
{
}

void PreviewTextColumn::draw()
{
    auto scrollArea = CScrollAreaBuilder::begin()
                          ->scrollY(true)
                          ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                          ->commence();

    auto textColumn = CColumnLayoutBuilder::begin()
                          ->gap(2)
                          ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                          ->commence();

    scrollArea->addChild(textColumn);
    layout_->addChild(scrollArea);

    layout_->setRepositioned([this, textColumn]() {
        if (this->contentLoaded_)
            return;

        const int visibleRows = computeVisibleRows(layout_, backend_);
        const std::size_t visibleColumns = computeVisibleColumns(layout_, backend_);
        const auto preview = readPreviewLines(path_, static_cast<std::size_t>(visibleRows), kMaxPreviewBytes, visibleColumns);

        auto makeStatusText = [this, textColumn](std::string text, Hyprtoolkit::CFontSize fontSize,
                                                 Hyprtoolkit::eFontAlignment alignment)
        {
            textColumn->addChild(CTextBuilder::begin()
                                     ->text(std::move(text))
                                     ->fontSize(std::move(fontSize))
                                     ->align(alignment)
                                     ->color([this]() -> Hyprtoolkit::CHyprColor
                                            { return backend_->getPalette()->m_colors.text; })
                                     ->commence());
        };

        if (!preview.success)
        {
            makeStatusText("This text file could not be previewed", Hyprtoolkit::CFontSize{Hyprtoolkit::CFontSize::HT_FONT_TEXT},
                           Hyprtoolkit::HT_FONT_ALIGN_CENTER);
            this->contentLoaded_ = true;
            return;
        }

        if (preview.lines.empty())
        {
            makeStatusText("This file is empty", Hyprtoolkit::CFontSize{Hyprtoolkit::CFontSize::HT_FONT_TEXT},
                           Hyprtoolkit::HT_FONT_ALIGN_CENTER);
        }
        else
        {
            for (auto &line : preview.lines)
            {
                textColumn->addChild(makePreviewLineRow(std::move(line),
                                                        Hyprtoolkit::CFontSize{Hyprtoolkit::CFontSize::HT_FONT_TEXT},
                                                        [this]() -> Hyprtoolkit::CHyprColor
                                                        { return backend_->getPalette()->m_colors.text; }));
            }
        }

        if (preview.truncated)
        {
            textColumn->addChild(makePreviewLineRow("... (preview truncated)",
                                                    Hyprtoolkit::CFontSize{Hyprtoolkit::CFontSize::HT_FONT_SMALL},
                                                    [this]() -> Hyprtoolkit::CHyprColor
                                                    { return backend_->getPalette()->m_colors.text.darken(0.2F); }));
        }

        this->contentLoaded_ = true;
    });
}
