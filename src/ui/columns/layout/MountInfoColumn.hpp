#pragma once

#include "BaseLayoutColumn.hpp"
#include "../../../core/MountInfoService.hpp"

#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/ScrollArea.hpp>

#include <vector>

namespace hyprfile::UI
{
    struct MountInfoColumnContent
    {
        Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CScrollAreaElement> root;
        Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CColumnLayoutElement> content;
        std::vector<Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CRowLayoutElement>> textRows;
        std::vector<Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CRectangleElement>> separators;
    };

    MountInfoColumnContent makeMountInfoColumnContent(Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> backend,
                                                       const std::vector<MountInfo>& mounts);
}

class MountInfoColumn : public BaseLayoutColumn
{
public:
    MountInfoColumn(CSharedPointer<IBackend> backend, float widthPercent);

    void draw() override;
};
