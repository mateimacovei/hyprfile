#pragma once

#include <filesystem>
#include <vector>
#include <iostream>
#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/types/SizeType.hpp>
#include <hyprtoolkit/element/Text.hpp>

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>

#include "../../model/FileItemLayout.hpp"

using namespace Hyprutils::Memory;
using namespace Hyprtoolkit;

#define SP CSharedPointer
#define WP CWeakPointer
#define UP CUniquePointer

class BaseLayoutColumn
{
public:
    virtual void draw() = 0;

    SP<CColumnLayoutElement> getLayout() const
    {
        return layout_;
    }
    std::filesystem::path getPath() const
    {
        return path_;
    }

    virtual ~BaseLayoutColumn() = default;

    /**
     * Re-synchronize pool/viewport after a resize or layout change.
     * Override in DirectoryColumn to adjust pool size to match the new viewport.
     */
    virtual void resync() {}

    /**
     * only for the case of directory preview columns. Otherwise, it will do nothing
     */
    virtual WP<FileItem> setSelection(const std::filesystem::path &path)
    {
        return WP<FileItem>{};
    }
    /**
     * only for the case of directory preview columns. Otherwise, it will do nothing
     */
    virtual WP<FileItem> getSelection() const
    {
        return WP<FileItem>{};
    }

    virtual bool isFullscreen() const { return false; }
    // go to child or get hte next image or skip forewards in video
    virtual void searchRight() {}
    // go to parent or get the previous image or go backwards in video
    virtual void searchLeft() {}

protected:
    const SP<IBackend> backend_;
    const std::filesystem::path path_;
    const SP<CColumnLayoutElement> layout_;

    BaseLayoutColumn(SP<IBackend> backend, std::filesystem::path path, float widthPercent)
        : backend_(backend), path_(std::move(path)),
          layout_(CColumnLayoutBuilder::begin()
                      ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {widthPercent, 1.F}})
                      ->commence())
    {
    }
};
