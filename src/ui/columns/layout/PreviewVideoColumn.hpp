#pragma once

#include "BaseLayoutColumn.hpp"

#include <hyprtoolkit/element/Image.hpp>

class PreviewVideoColumn : public BaseLayoutColumn
{
public:
    PreviewVideoColumn(SP<IBackend> backend, std::filesystem::path path, float widthPercent, bool isFullscreen = false);

    void draw() override;

    bool isFullscreen() const override { return is_fullscreen_; }

private:
    SP<CImageElement> videoFrame_;
    SP<CColumnLayoutElement> container_;

    bool contentLoaded_ = false;
    const bool is_fullscreen_;
};
