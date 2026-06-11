#pragma once

#include "DirectoryColumn.hpp"

class PreviewDirectoryColumn :  public DirectoryColumn
{
public:
    PreviewDirectoryColumn(CSharedPointer<IBackend> backend, std::filesystem::path path, float widthPercent);

};
