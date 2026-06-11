#pragma once

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/types/SizeType.hpp>

#include <hyprutils/memory/SharedPtr.hpp>

using namespace Hyprutils::Memory;
using namespace Hyprtoolkit;

#define SP CSharedPointer
#define WP CWeakPointer
#define UP CUniquePointer

class StatusBarBase
{
public:
    virtual ~StatusBarBase() = default;

    SP<CRowLayoutElement> getLayout() const
    {
        return layout_;
    }

protected:
    const SP<IBackend> backend_;
    const SP<CRowLayoutElement> layout_;

    StatusBarBase(SP<IBackend> backend, SP<CRowLayoutElement> layout)
        : backend_(backend), layout_(std::move(layout))
    {
    }

    static SP<CRowLayoutElement> makeLayout(int gap = 0)
    {
        auto layout = CRowLayoutBuilder::begin()
                          ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_ABSOLUTE, {1.F, 30.F}})
                          ->gap(gap)
                          ->commence();
        return layout;
    }
};
