#pragma once

#include "BaseLayoutColumn.hpp"
#include "../../../core/FileSystemService.hpp"

#include <optional>
#include <string>
#include <vector>

class DirectoryColumn : public BaseLayoutColumn
{
public:
    struct PasteSelectionResult
    {
        WP<FileItem> selection;
        bool refreshedDirectory = false;
    };

    DirectoryColumn(CSharedPointer<IBackend> backend, bool empty, std::filesystem::path path, float widthPercent);
    virtual ~DirectoryColumn() = default;

    void draw() override;
    /**
     * Re-synchronize pool/viewport after a resize or layout change.
     * Override in DirectoryColumn to adjust pool size to match the new viewport.
     */
    void resync() override
    {
        if (entries_.empty() || items_size <= 0)
            return;

        // Re-sync pool to match current viewport, keeping selection
        updateVisibleRange(true);
    }

    int getTotalItemsCount() const
    {
        return items_size;
    }

    bool empty() const
    {
        return empty_;
    }

    WP<FileItem> getSelection() const override
    {
        if (selectedIndex_ < 0 || selectedIndex_ >= items_size)
            return WP<FileItem>{};

        return (entries_)[selectedIndex_];
    }

    WP<FileItem> setSelection(int newIndex, bool requireFullscreenSupport);
    WP<FileItem> setSelection(const std::filesystem::path &path) override;

    WP<FileItem> moveDown(int count, bool requireFullscreenSupport)
    {

        if (!requireFullscreenSupport)
        {
            return setSelection(selectedIndex_ + count, false);
        }
        else
        {
            // Skip to the Nth item with fullscreen support, if possible. If not, go to the last item with fullscreen support
            int found = 0;
            int last_fullscreen_item_index = -1;
            for (int i = selectedIndex_ + 1; i < items_size; i++)
            {
                if (entries_[i]->fullscreenSupport())
                {
                    last_fullscreen_item_index = i;
                    if (++found >= count)
                        return setSelection(i, true);
                }
            }
            if (last_fullscreen_item_index >= 0)
            {
                return setSelection(last_fullscreen_item_index, true);
            }
            return WP<FileItem>{};
        }
    }

    WP<FileItem> moveUp(int count, bool requireFullscreenSupport)
    {

        if (!requireFullscreenSupport)
        {
            return setSelection(selectedIndex_ - count, false);
        }
        else
        {
            // Skip to the Nth previous item with fullscreen support if possible. If not, go to the first item with fullscreen support
            int found = 0;
            int first_fullscreen_item_index = -1;
            for (int i = selectedIndex_ - 1; i >= 0; i--)
            {
                if (entries_[i]->fullscreenSupport())
                {
                    first_fullscreen_item_index = i;
                    if (++found >= count)
                        return setSelection(i, true);
                }
            }
            if (first_fullscreen_item_index >= 0)
            {
                return setSelection(first_fullscreen_item_index, true);
            }
            return WP<FileItem>{};
        }
    }

    WP<FileItem> pageDown(bool requireFullscreenSupport)
    {
        const int step = computePageStep();
        return moveDown(step, requireFullscreenSupport);
    }
    WP<FileItem> pageUp(bool requireFullscreenSupport)
    {
        const int step = computePageStep();
        return moveUp(step, requireFullscreenSupport);
    }
    WP<FileItem> toggleHidden();
    WP<FileItem> refresh();
    // return pointer to the new preview item, or an empty pointer if there was no preview / the only item in the folder was deleted.
    WP<FileItem> trash();
    void search() {}
    void setSearchQuery(std::string query);
    void clearSearch();
    WP<FileItem> commitSearch();
    WP<FileItem> jumpToNextSearchResult(int count = 1);
    WP<FileItem> jumpToPreviousSearchResult(int count = 1);
    WP<FileItem> startSelection();
    bool hasMultiSelection() const;
    WP<FileItem> cancelMultiSelection();
    void openTerminal();
    WP<FileItem> copySelection();
    WP<FileItem> cutSelection();
    PasteSelectionResult pasteSelection();

protected:
    // absolute index of the selected element (from 0 to entries_.size() - 1). -1 if no selection
    int selectedIndex_ = -1;
    const SP<CColumnLayoutElement> fileList_;

private:
    static constexpr float kItemHeight = 20.F;

    struct MultiSelectionState
    {
        int sourceIndex = -1;
        std::filesystem::path sourcePath;
    };

    // Lightweight data for ALL directory entries (just paths)
    std::vector<SP<FileItem>> entries_;
    std::string searchQuery_;
    std::optional<MultiSelectionState> multiSelection_;
    // Total number of entries in the directory (not pool size)
    int items_size;

    // Fixed pool of FileItem UI elements (only enough for the visible viewport)
    std::vector<SP<FileItemLayout>> pool_;
    int poolSize_ = 0;
    // true if the poolSize_ field has the correct value for the current layout. not necessarely connected to pool_.size()
    bool correctPoolSize_ = false;

    // Index into entries_ of the first item currently bound to pool_[0]
    int visibleStart_ = 0;

    // return true if the view needs to be resized
    bool computeViewportSlots(bool size_change);
    int computePageStep();
    void syncPool(bool size_change);
    void updateVisibleRange(bool size_change);
    WP<FileItem> reloadEntries(const std::filesystem::path* preferredSelection, int fallbackIndex);
    void rebuildFileList();
    void renderEmptyState();
    bool entryMatchesSearch(int index) const;
    WP<FileItem> jumpToSearchResult(int count, int direction, const char *actionName);
    void updateVisibleSearchMatches();
    bool isMultiSelectedIndex(int index) const;
    std::vector<std::filesystem::path> selectedOperationSources() const;
    void updateVisibleMultiSelection();
    void reconcileMultiSelectionSource();
    std::optional<std::filesystem::path> nextSelectionAfterOperation() const;

    const bool empty_;
};
