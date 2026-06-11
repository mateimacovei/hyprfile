#pragma once

#include "layout/DirectoryColumn.hpp"
#include "BaseStructureColumn.hpp"

class ParentColumn : public BaseStructureColumn
{
public:
    ParentColumn(CSharedPointer<IBackend> backend, float widthPercent);

    SP<BaseLayoutColumn> instantiateParentColumn(std::filesystem::path &cwd);
    void set_layout_column(SP<BaseLayoutColumn> newColumn);
    WP<FileItem> setSelection(const std::filesystem::path &path);
    SP<BaseLayoutColumn> get_layout_column() const override;
    SP<DirectoryColumn> get_layout_directory_column() const;

private:
    SP<BaseLayoutColumn> column_;
    SP<DirectoryColumn> directoryColumn_;
};
