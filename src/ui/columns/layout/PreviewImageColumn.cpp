#include "PreviewImageColumn.hpp"

#include <hyprtoolkit/element/Image.hpp>
#include <hyprtoolkit/element/Text.hpp>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <algorithm>
#include <filesystem>
#include <optional>

struct ImageDimensions
{
    int width;
    int height;
};

static std::optional<ImageDimensions> readImageDimensions(const std::filesystem::path &path)
{
    int w, h;
    GdkPixbufFormat *fmt = gdk_pixbuf_get_file_info(path.c_str(), &w, &h);
    if (!fmt)
        return std::nullopt;
    return ImageDimensions{w, h};
}

PreviewImageColumn::PreviewImageColumn(CSharedPointer<IBackend> backend, std::filesystem::path path, float widthPercent, bool isFullscreen)
    : BaseLayoutColumn(backend, std::move(path), widthPercent), is_fullscreen_(isFullscreen)
{
}

void PreviewImageColumn::draw()
{
    container_ = CColumnLayoutBuilder::begin()
                     ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                     ->commence();

    layout_->addChild(container_);
    auto loadImage = [this]()
    {
        if (!container_ || this->contentLoaded_)
            return;

        const auto containerSize = container_->size();
        if (containerSize.x <= 0.F || containerSize.y <= 0.F)
            return;

        const auto dims = readImageDimensions(path_);
        if (!dims || dims->width <= 0 || dims->height <= 0)
        {
            container_->addChild(CTextBuilder::begin()
                                     ->text("This image could not be previewed")
                                     ->align(Hyprtoolkit::HT_FONT_ALIGN_CENTER)
                                     ->color([this]() -> Hyprtoolkit::CHyprColor
                                             { return backend_->getPalette()->m_colors.text; })
                                     ->commence());
            this->contentLoaded_ = true;
            return;
        }

        const double scale = std::min(containerSize.x / dims->width, containerSize.y / dims->height);
        const Hyprutils::Math::Vector2D imageSize = {dims->width * scale, dims->height * scale};

        auto image = CImageBuilder::begin()
                         ->path(path_.string())
                         ->fitMode(Hyprtoolkit::IMAGE_FIT_MODE_STRETCH)
                         ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, imageSize})
                         ->commence();

        if (!image)
        {
            container_->addChild(CTextBuilder::begin()
                                     ->text("This image could not be previewed")
                                     ->align(Hyprtoolkit::HT_FONT_ALIGN_CENTER)
                                     ->color([this]() -> Hyprtoolkit::CHyprColor
                                             { return backend_->getPalette()->m_colors.text; })
                                     ->commence());
            this->contentLoaded_ = true;
            return;
        }

        container_->addChild(image);
        this->contentLoaded_ = true;
    };

    container_->setRepositioned([this, loadImage]() mutable
                                { loadImage(); });
}
