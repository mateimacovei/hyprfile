#pragma once

#include <optional>

#include "layout/BaseLayoutColumn.hpp"
#include "layout/DirectoryColumn.hpp"

class BaseStructureColumn
{
public:
    void draw()
    {
        // call the draw method on the base layout column if the safe pointer does not cintain an empty pointer
        if (auto col = get_layout_column(); col)
        {
            col->draw();
        }
    }

    void resync()
    {
        if (auto col = get_layout_column(); col)
        {
            col->resync();
        }
    }

    SP<IElement> getLayout() const
    {
        if (auto col = get_layout_column(); col)
        {
            return col->getLayout();
        }
        return nullptr;
    }

    virtual SP<BaseLayoutColumn> get_layout_column() const = 0;

    virtual ~BaseStructureColumn() = default;

protected:
    // these 2 serve as parameters for the new preview columns which will be created
    const SP<IBackend> backend_;
    const float width_percent_;

    BaseStructureColumn(SP<IBackend> backend, float widthPercent) : backend_(backend), width_percent_(widthPercent) {}
};

class BaseDirectoryStructureColumn : public BaseStructureColumn
{
protected:
    // can be empty, if the current directory is the linux root, then there is no parent layout
    SP<DirectoryColumn> column_;

    BaseDirectoryStructureColumn(SP<IBackend> backend, float widthPercent) : BaseStructureColumn(backend, widthPercent), column_(nullptr) {}

public:
    // COLUMN MANAGEMENT
    void set_layout_column(SP<DirectoryColumn> new_column)
    {
        column_ = new_column;
    }

    // ACTIONS
    WP<FileItem> setSelection(int newIndex, bool requireFullscreenSupport)
    {
        return column_->setSelection(newIndex, requireFullscreenSupport);
    }

    WP<FileItem> setSelection(const std::filesystem::path &path)
    {
        return column_->setSelection(path);
    }

    SP<BaseLayoutColumn> get_layout_column() const override
    {
        return column_;
    }
    SP<DirectoryColumn> get_layout_directory_column() const
    {
        return column_;
    }
};
