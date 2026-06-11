#pragma once

#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/types/SizeType.hpp>
#include <hyprutils/memory/SharedPtr.hpp>

namespace hyprfile::UI
{
    inline Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CRowLayoutElement> makeMainLayout()
    {
        return Hyprtoolkit::CRowLayoutBuilder::begin()
            ->size({Hyprtoolkit::CDynamicSize::HT_SIZE_PERCENT, Hyprtoolkit::CDynamicSize::HT_SIZE_ABSOLUTE, {1.F, 0.F}})
            ->commence();
    }
}
