#include "PreviewDirectoryColumn.hpp"

PreviewDirectoryColumn::PreviewDirectoryColumn(CSharedPointer<IBackend> backend, std::filesystem::path path, float widthPercent)
    : DirectoryColumn(backend, false, std::move(path), widthPercent)
{
}

