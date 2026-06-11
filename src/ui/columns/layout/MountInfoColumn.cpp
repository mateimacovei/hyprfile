#include "MountInfoColumn.hpp"

#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/ScrollArea.hpp>

#include <sstream>
#include <utility>

namespace
{
    SP<CRowLayoutElement> makeMountTextRow(std::string text, Hyprtoolkit::CFontSize fontSize, Hyprtoolkit::colorFn color)
    {
        auto row = CRowLayoutBuilder::begin()
                       ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                       ->commence();

        row->addChild(CTextBuilder::begin()
                          ->text(std::move(text))
                          ->fontSize(std::move(fontSize))
                          ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                          ->align(Hyprtoolkit::HT_FONT_ALIGN_LEFT)
                          ->color(std::move(color))
                          ->commence());

        return row;
    }

    std::string formatUsageLine(const MountInfo& mount)
    {
        if (!mount.totalBytes || !mount.usedBytes)
            return "Storage unavailable";

        return formatStorageBytes(*mount.usedBytes) + " used / " + formatStorageBytes(*mount.totalBytes);
    }

    std::string formatMountPointLine(const std::vector<std::filesystem::path>& mountPoints)
    {
        std::ostringstream out;
        for (std::size_t i = 0; i < mountPoints.size(); ++i)
        {
            if (i > 0)
                out << ", ";
            out << mountPoints[i].string();
        }
        return out.str();
    }

    SP<CRectangleElement> makeSeparator(SP<IBackend> backend)
    {
        return CRectangleBuilder::begin()
            ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_ABSOLUTE, {1.F, 1.F}})
            ->color([backend]() -> Hyprtoolkit::CHyprColor
                    { return backend->getPalette()->m_colors.text.darken(0.65F); })
            ->commence();
    }

    SP<CRectangleElement> makeMountBlock(SP<IBackend> backend, const MountInfo& mount,
                                         std::vector<SP<CRowLayoutElement>>& textRows)
    {
        auto block = CRectangleBuilder::begin()
                          ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                         ->color([backend]() -> Hyprtoolkit::CHyprColor
                                 { return backend->getPalette()->m_colors.background; })
                         ->rounding(8)
                         ->commence();

        auto textColumn = CColumnLayoutBuilder::begin()
                              ->gap(2)
                              ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                              ->commence();
        textColumn->setMargin(4);

        auto addTextRow = [&](std::string text, Hyprtoolkit::CFontSize fontSize, Hyprtoolkit::colorFn color)
        {
            auto row = makeMountTextRow(std::move(text), std::move(fontSize), std::move(color));
            textRows.push_back(row);
            textColumn->addChild(row);
        };

        addTextRow(mount.displayName,
                   Hyprtoolkit::CFontSize{Hyprtoolkit::CFontSize::HT_FONT_TEXT},
                   [backend]() -> Hyprtoolkit::CHyprColor
                   { return backend->getPalette()->m_colors.text; });
        addTextRow(formatUsageLine(mount),
                   Hyprtoolkit::CFontSize{Hyprtoolkit::CFontSize::HT_FONT_SMALL},
                   [backend]() -> Hyprtoolkit::CHyprColor
                   { return backend->getPalette()->m_colors.text.darken(0.2F); });
        addTextRow(formatMountPointLine(mount.mountPoints),
                   Hyprtoolkit::CFontSize{Hyprtoolkit::CFontSize::HT_FONT_SMALL},
                   [backend]() -> Hyprtoolkit::CHyprColor
                   { return backend->getPalette()->m_colors.text.darken(0.35F); });

        block->addChild(textColumn);
        return block;
    }
}

hyprfile::UI::MountInfoColumnContent hyprfile::UI::makeMountInfoColumnContent(SP<IBackend> backend,
                                                                                 const std::vector<MountInfo>& mounts)
{
    MountInfoColumnContent layout;
    layout.root = CScrollAreaBuilder::begin()
                      ->scrollY(true)
                      ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                      ->commence();

    layout.content = CColumnLayoutBuilder::begin()
                         ->gap(6)
                         ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                         ->commence();

    layout.root->addChild(layout.content);

    if (mounts.empty())
    {
        auto row = makeMountTextRow("No mounted disks found",
                                    Hyprtoolkit::CFontSize{Hyprtoolkit::CFontSize::HT_FONT_TEXT},
                                    [backend]() -> Hyprtoolkit::CHyprColor
                                    { return backend->getPalette()->m_colors.text; });
        layout.textRows.push_back(row);
        layout.content->addChild(row);
        return layout;
    }

    for (std::size_t i = 0; i < mounts.size(); ++i)
    {
        if (i > 0)
        {
            auto separator = makeSeparator(backend);
            layout.separators.push_back(separator);
            layout.content->addChild(separator);
        }

        layout.content->addChild(makeMountBlock(backend, mounts[i], layout.textRows));
    }

    return layout;
}

MountInfoColumn::MountInfoColumn(CSharedPointer<IBackend> backend, float widthPercent)
    : BaseLayoutColumn(std::move(backend), "/", widthPercent)
{
}

void MountInfoColumn::draw()
{
    auto content = hyprfile::UI::makeMountInfoColumnContent(backend_, MountInfoService::listMounts());
    layout_->addChild(content.root);
}
