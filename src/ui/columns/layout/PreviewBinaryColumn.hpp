#pragma once

#include "BaseLayoutColumn.hpp"

class PreviewBinaryColumn : public BaseLayoutColumn
{
public:
    PreviewBinaryColumn(SP<IBackend> backend, std::filesystem::path path, float widthPercent);

    void draw() override;
};
