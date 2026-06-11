#include "PreviewVideoColumn.hpp"
#include "../../../video/VideoThumbnail.hpp"

#include <hyprtoolkit/element/Image.hpp>
#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/element/Element.hpp>
#include <hyprtoolkit/system/Icons.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>

PreviewVideoColumn::PreviewVideoColumn(CSharedPointer<IBackend> backend, std::filesystem::path path, float widthPercent, bool isFullscreen)
    : BaseLayoutColumn(backend, std::move(path), widthPercent), is_fullscreen_(isFullscreen)
{
}

void PreviewVideoColumn::draw()
{
    container_ = CColumnLayoutBuilder::begin()
                     ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                     ->commence();

    layout_->addChild(container_);

    container_->setRepositioned([this]()
                                {
        if (contentLoaded_)
            return;

        if (!container_)
            return;

        const auto containerSize = container_->size();
        if (containerSize.x <= 0.F || containerSize.y <= 0.F)
            return;

        const int maxW = static_cast<int>(containerSize.x);
        const int maxH = static_cast<int>(containerSize.y);

        // Extract synchronously because Hyprtoolkit UI mutations must stay on the main thread.
        auto thumbnail = VideoThumbnail::extractToPngData(path_, maxW, maxH);
        if (thumbnail.pngData.empty() || thumbnail.width <= 0 || thumbnail.height <= 0)
        {
            container_->addChild(CTextBuilder::begin()
                                      ->text("This video could not be previewed")
                                      ->align(Hyprtoolkit::HT_FONT_ALIGN_CENTER)
                                      ->color([this]() -> Hyprtoolkit::CHyprColor
                                              { return backend_->getPalette()->m_colors.text; })
                                      ->commence());
            contentLoaded_ = true;
            return;
        }

        // Scale to fit container, preserving aspect ratio.
        const double scale = std::min(containerSize.x / thumbnail.width, containerSize.y / thumbnail.height);
        const Hyprutils::Math::Vector2D imageSize = {thumbnail.width * scale, thumbnail.height * scale};

        videoFrame_ = CImageBuilder::begin()
                          ->data(std::move(thumbnail.pngData))
                          ->sync(true)
                          ->fitMode(Hyprtoolkit::IMAGE_FIT_MODE_STRETCH)
                          ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, imageSize})
                          ->commence();

        if (!videoFrame_)
        {
            container_->addChild(CTextBuilder::begin()
                                      ->text("This video could not be previewed")
                                      ->align(Hyprtoolkit::HT_FONT_ALIGN_CENTER)
                                      ->color([this]() -> Hyprtoolkit::CHyprColor
                                              { return backend_->getPalette()->m_colors.text; })
                                      ->commence());
            contentLoaded_ = true;
            return;
        }

        container_->addChild(videoFrame_);

        // Centered play glyph overlay to differentiate videos from images.
        auto iconFactory = backend_->systemIcons();
        if (!iconFactory)
        {
            std::cerr << "[hyprfile] video preview overlay: system icon factory unavailable\n";
        }
        else
        {
            const std::array<const char *, 4> iconCandidates = {
                "media-playback-start",
                "media-playback-start-symbolic",
                "player-play",
                "media-playback-play"};

            SP<ISystemIconDescription> playIconDesc = nullptr;
            for (const auto *name : iconCandidates)
            {
                auto desc = iconFactory->lookupIcon(name);
                if (desc && desc->exists())
                {
                    playIconDesc = desc;
                    break;
                }
            }

            if (!playIconDesc)
            {
                std::cerr << "[hyprfile] video preview overlay: play icon not found in theme (tried media-playback-start, media-playback-start-symbolic, player-play, media-playback-play)\n";
            }
            else
            {
                auto playIcon = CImageBuilder::begin()
                                     ->icon(playIconDesc)
                                     ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, {20.F, 20.F}})
                                     ->commence();

                if (playIcon)
                {
                    auto glyphBackground = CRectangleBuilder::begin()
                                              ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, {52.F, 52.F}})
                                              ->color([]() -> Hyprtoolkit::CHyprColor
                                                      { return Hyprtoolkit::CHyprColor(0.F, 0.F, 0.F, 0.F); })
                                              ->borderColor([this]() -> Hyprtoolkit::CHyprColor
                                                           { return backend_->getPalette()->m_colors.background; })
                                              ->borderThickness(2)
                                              ->rounding(14)
                                              ->commence();

                    glyphBackground->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
                    glyphBackground->setPositionFlag(IElement::HT_POSITION_FLAG_CENTER, true);

                    playIcon->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
                    playIcon->setPositionFlag(IElement::HT_POSITION_FLAG_CENTER, true);
                    playIcon->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
                    playIcon->setPositionFlag(IElement::HT_POSITION_FLAG_VCENTER, true);

                    glyphBackground->addChild(playIcon);
                    videoFrame_->addChild(glyphBackground);
                }
            }
        }
        contentLoaded_ = true; });
}
