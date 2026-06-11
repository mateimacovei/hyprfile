#include "PreviewColumn.hpp"
#include "layout/PreviewDirectoryColumn.hpp"
#include "layout/PreviewTextColumn.hpp"
#include "layout/PreviewImageColumn.hpp"
#include "layout/PreviewVideoColumn.hpp"
#include "layout/PreviewBinaryColumn.hpp"

#include <hyprutils/memory/SharedPtr.hpp>

PreviewColumn::PreviewColumn(CSharedPointer<IBackend> backend, float widthPercent)
    : BaseStructureColumn(backend, widthPercent), column_(nullptr)
{
}

SP<BaseLayoutColumn> PreviewColumn::createPreview(const SP<FileItem> &item)
{
    if (!item)
        return nullptr;

    FileItem::FileItemType type = item->getPreviewType();
    const std::filesystem::path path = item->getPath();
    switch (type)
    {
    case FileItem::FileItemType::Directory:
        return makeShared<PreviewDirectoryColumn>(backend_, path, width_percent_);
    case FileItem::FileItemType::Image:
        return makeShared<PreviewImageColumn>(backend_, path, width_percent_);
    case FileItem::FileItemType::Video:
        return makeShared<PreviewVideoColumn>(backend_, path, width_percent_);
    case FileItem::FileItemType::Text:
        return makeShared<PreviewTextColumn>(backend_, path, width_percent_);
    case FileItem::FileItemType::Binary:
    default:
        return makeShared<PreviewBinaryColumn>(backend_, path, width_percent_);
    }
}

SP<BaseLayoutColumn> PreviewColumn::createFullscreenPreview(const SP<FileItem> &item)
{
    if (!item)
        return nullptr;

    FileItem::FileItemType type = item->getPreviewType();
    const std::filesystem::path path = item->getPath();
    constexpr float fullWidth = 1.0F;
    switch (type)
    {
    case FileItem::FileItemType::Image:
        return makeShared<PreviewImageColumn>(backend_, path, fullWidth, true);
    case FileItem::FileItemType::Video:
        return makeShared<PreviewVideoColumn>(backend_, path, fullWidth, true);
    default:
    {
        // print error message and return empty SP
        std::cerr << "No fullscreen preview available for this file type: " << path << '\n';
        return nullptr;
    }
    }
}
