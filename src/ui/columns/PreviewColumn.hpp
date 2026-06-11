#pragma once

#include "layout/PreviewDirectoryColumn.hpp"
#include "BaseStructureColumn.hpp"
#include "../model/FileItemLayout.hpp"

class PreviewColumn : public BaseStructureColumn
{
public:
    explicit PreviewColumn(CSharedPointer<IBackend> backend, float widthPercent);

    /**
     * create preview column for directory or file
     */
    SP<BaseLayoutColumn> createPreview(const SP<FileItem> &item);

    /**
     * Create a fullscreen (100% width) preview column for the given item.
     */
    SP<BaseLayoutColumn> createFullscreenPreview(const SP<FileItem> &item);

    SP<BaseLayoutColumn> get_layout_column() const override
    {
        return column_;
    }

    void set_layout_column(SP<BaseLayoutColumn> new_column)
    {
        column_ = new_column;
    }

    bool isFullscreen() const
    {
        return column_ && column_->isFullscreen();
    }

    /**
     * only for the case of directory preview columns. Otherwise, it will do nothing
     */
    WP<FileItem> setSelection(const std::filesystem::path &path)
    {
        return column_->setSelection(path);
    }

private:
    // nullable
    SP<BaseLayoutColumn> column_;
};
