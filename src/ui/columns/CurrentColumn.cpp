#include "CurrentColumn.hpp"

CurrentColumn::CurrentColumn(CSharedPointer<IBackend> backend, float widthPercent) : BaseDirectoryStructureColumn(backend, widthPercent)
{
}

SP<DirectoryColumn> CurrentColumn::instantiateColumn(std::filesystem::path &cwd)
{
    return makeShared<DirectoryColumn>(backend_, false, cwd, width_percent_);
}
