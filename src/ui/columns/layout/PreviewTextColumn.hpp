#pragma once

#include "BaseLayoutColumn.hpp"

class PreviewTextColumn : public BaseLayoutColumn
{
public:
    PreviewTextColumn(CSharedPointer<IBackend> backend, std::filesystem::path path, float widthPercent);

    void draw() override;

private:
    bool contentLoaded_ = false;
};
