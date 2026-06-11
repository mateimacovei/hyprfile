#pragma once

#include "FileItem.hpp"

CHyprColor fileItemMultiSelectionIndicatorColor(SP<IBackend> backend, bool selected, bool multiSelected);

class FileItemLayout
{
public:
    FileItemLayout(CSharedPointer<IBackend> backend, SP<FileItem> item);

    void setSelected(bool selected);
    void setMultiSelected(bool multiSelected);
    void setSearchMatched(bool matched);
    void rebind(SP<FileItem> newItem, bool selected, bool multiSelected);

    const SP<FileItem> getItem() const
    {
        return item_;
    }

    FileItem::FileItemType getPreviewType() const
    {
        return item_->getPreviewType();
    }

    const std::filesystem::path &getPath() const
    {
        return item_->getPath();
    }

    std::filesystem::perms getPermissions() const
    {
        return item_->getPermissions();
    }

    bool isSymlink() const
    {
        return item_->isSymlink();
    }

    CSharedPointer<CRectangleElement> getLayout() const
    {
        return background_;
    }

private:
    bool selected_ = false;
    bool multiSelected_ = false;
    bool searchMatched_ = false;
    const SP<IBackend> backend_;
    SP<FileItem> item_;
    const SP<CRectangleElement> background_;
    const SP<CRectangleElement> multiSelectionIndicator_;
    const SP<CRowLayoutElement> contentLayout_;
    SP<CImageElement> iconElement_;
    SP<CColumnLayoutElement> iconPadding_;
    SP<CTextElement> textElement_;
    SP<CTextElement> buildTextElement() const;
    void applySelectionStyle();
    void applyMultiSelectionStyle();
};
