#pragma once

#include "layout/DirectoryColumn.hpp"
#include "BaseStructureColumn.hpp"

#include <utility>

class CurrentColumn : public BaseDirectoryStructureColumn
{
public:
    CurrentColumn(SP<IBackend> backend, float widthPercent);

    SP<DirectoryColumn> instantiateColumn(std::filesystem::path &cwd);

    WP<FileItem> moveDown(int count, bool requireFullscreenSupport)
    {
        return column_->moveDown(count, requireFullscreenSupport);
    }
    WP<FileItem> moveUp(int count, bool requireFullscreenSupport)
    {
        return column_->moveUp(count, requireFullscreenSupport);
    }
    WP<FileItem> goToTop(bool requireFullscreenSupport)
    {
        return column_->setSelection(0, requireFullscreenSupport);
    }
    WP<FileItem> goToBottom(bool requireFullscreenSupport)
    {
        return column_->setSelection(column_->getTotalItemsCount() - 1, requireFullscreenSupport);
    }
    WP<FileItem> pageDown(bool requireFullscreenSupport)
    {
        return column_->pageDown(requireFullscreenSupport);
    }
    WP<FileItem> pageUp(bool requireFullscreenSupport)
    {
        return column_->pageUp(requireFullscreenSupport);
    }
    WP<FileItem> toggleHidden()
    {
        return column_->toggleHidden();
    }
    WP<FileItem> refresh()
    {
        return column_->refresh();
    }
    WP<FileItem> trash()
    {
        return column_->trash();
    }
    void search()
    {
        column_->search();
    }
    void setSearchQuery(std::string query)
    {
        column_->setSearchQuery(std::move(query));
    }
    void clearSearch()
    {
        column_->clearSearch();
    }
    WP<FileItem> commitSearch()
    {
        return column_->commitSearch();
    }
    WP<FileItem> jumpToNextSearchResult(int count)
    {
        return column_->jumpToNextSearchResult(count);
    }
    WP<FileItem> jumpToPreviousSearchResult(int count)
    {
        return column_->jumpToPreviousSearchResult(count);
    }
    WP<FileItem> startSelection()
    {
        return column_->startSelection();
    }
    bool hasMultiSelection() const
    {
        return column_ && column_->hasMultiSelection();
    }
    WP<FileItem> cancelMultiSelection()
    {
        return column_ ? column_->cancelMultiSelection() : WP<FileItem>{};
    }
    void openTerminal()
    {
        column_->openTerminal();
    }
    WP<FileItem> copySelection()
    {
        return column_->copySelection();
    }
    WP<FileItem> cutSelection()
    {
        return column_->cutSelection();
    }
    DirectoryColumn::PasteSelectionResult pasteSelection()
    {
        return column_->pasteSelection();
    }
};
