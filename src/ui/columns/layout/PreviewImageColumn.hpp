#pragma once

#include "BaseLayoutColumn.hpp"

class PreviewImageColumn : public BaseLayoutColumn
{
public:
    PreviewImageColumn(CSharedPointer<IBackend> backend, std::filesystem::path path, float widthPercent, bool isFullscreen = false);

    void draw() override;

    bool isFullscreen() const override { return is_fullscreen_; }

private:
    SP<CColumnLayoutElement> container_;
    bool contentLoaded_ = false;
    const bool is_fullscreen_;
};
