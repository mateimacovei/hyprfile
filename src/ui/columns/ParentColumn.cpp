#include "ParentColumn.hpp"

#include "layout/MountInfoColumn.hpp"

ParentColumn::ParentColumn(CSharedPointer<IBackend> backend, float widthPercent)
    : BaseStructureColumn(backend, widthPercent), column_(nullptr), directoryColumn_(nullptr)
{
}

SP<BaseLayoutColumn> ParentColumn::instantiateParentColumn(std::filesystem::path &cwd)
{
    std::filesystem::path parent_path = cwd.parent_path();
    if (parent_path == cwd)
    {
        directoryColumn_ = nullptr;
        return makeShared<MountInfoColumn>(backend_, width_percent_);
    }

    directoryColumn_ = makeShared<DirectoryColumn>(backend_, false, parent_path, width_percent_);
    return directoryColumn_;
}

void ParentColumn::set_layout_column(SP<BaseLayoutColumn> newColumn)
{
    column_ = newColumn;
}

WP<FileItem> ParentColumn::setSelection(const std::filesystem::path &path)
{
    if (!directoryColumn_)
        return WP<FileItem>{};

    return directoryColumn_->setSelection(path);
}

SP<BaseLayoutColumn> ParentColumn::get_layout_column() const
{
    return column_;
}

SP<DirectoryColumn> ParentColumn::get_layout_directory_column() const
{
    return directoryColumn_;
}
