#include "DirectoryColumn.hpp"

#include "../../../core/FileOperationClipboard.hpp"
#include "../../../core/FileOperations.hpp"
#include "../../../core/TerminalLauncher.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <system_error>

namespace
{
    SP<CColumnLayoutElement> makeFileListBuilder()
    {
        return CColumnLayoutBuilder::begin()
            ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
            ->commence();
    }

    std::string lowercase(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
                       { return static_cast<char>(std::tolower(ch)); });
        return value;
    }

    std::filesystem::path normalizedAbsolute(const std::filesystem::path& path)
    {
        std::error_code ec;
        const auto absolute = std::filesystem::absolute(path, ec);
        if (ec)
            return path.lexically_normal();
        return absolute.lexically_normal();
    }

    bool refreshedDirectory(const FileOperationClipboard::State& state, const FileOperations::PasteResult& result)
    {
        if (result.destinations.empty())
            return false;

        if (result.destinations.size() != state.sources.size())
            return true;

        for (std::size_t i = 0; i < result.destinations.size(); ++i)
        {
            if (normalizedAbsolute(result.destinations[i]) != normalizedAbsolute(state.sources[i]))
                return true;
        }

        return false;
    }
}

DirectoryColumn::DirectoryColumn(CSharedPointer<IBackend> backend, bool empty, std::filesystem::path path, float widthPercent)
    : BaseLayoutColumn(backend, std::move(path), widthPercent),
      empty_(empty),
      fileList_(makeFileListBuilder()),
      items_size(0)
{
    layout_->addChild(fileList_);
}

void DirectoryColumn::draw()
{
    if (empty_)
        return;

    reloadEntries(nullptr, 0);
}

WP<FileItem> DirectoryColumn::reloadEntries(const std::filesystem::path* preferredSelection, int fallbackIndex)
{
    entries_.clear();
    pool_.clear();
    fileList_->clearChildren();
    items_size = 0;
    poolSize_ = 0;
    correctPoolSize_ = false;
    visibleStart_ = 0;
    selectedIndex_ = -1;

    auto dirEntries = FileSystemService::get().listDirectory(path_);
    if (dirEntries.empty())
    {
        multiSelection_.reset();
        renderEmptyState();
        return WP<FileItem>{};
    }

    // Populate lightweight entries (paths only, no UI elements)
    entries_.reserve(dirEntries.size());
    for (auto &entry : dirEntries)
        entries_.push_back(makeShared<FileItem>(entry.path));

    items_size = static_cast<int>(entries_.size());
    reconcileMultiSelectionSource();

    int newSelectedIndex = std::clamp(fallbackIndex, 0, items_size - 1);
    if (preferredSelection)
    {
        for (int i = 0; i < items_size; ++i)
        {
            if (entries_[i]->getPath() == *preferredSelection)
            {
                newSelectedIndex = i;
                break;
            }
        }
    }

    // handle visible items pool

    // We don't know the viewport size yet (layout hasn't happened).
    // Create a small initial pool — it will be resized on the first setSelection().
    // Use a conservative initial count; the real viewport-based size comes later.
    poolSize_ = std::min(items_size, 100);
    pool_.reserve(poolSize_);
    for (int i = 0; i < poolSize_; ++i)
    {
        pool_.push_back(makeShared<FileItemLayout>(backend_, entries_[i]));
        pool_.back()->setSearchMatched(entryMatchesSearch(i));
    }

    visibleStart_ = 0;
    selectedIndex_ = newSelectedIndex;
    rebuildFileList();
    updateVisibleRange(true);

    const int poolIndex = selectedIndex_ - visibleStart_;
    if (poolIndex >= 0 && poolIndex < static_cast<int>(pool_.size()))
        return pool_[poolIndex]->getItem();

    return entries_[selectedIndex_];
}

bool DirectoryColumn::computeViewportSlots(bool size_change)
{
    if (!size_change)
    {
        // if we have a difference between poolSize_ and the actual number of items in the pool, it means that the viewport size has changed since the last time we computed the pool size
        if (poolSize_ != static_cast<int>(pool_.size()))
        {
            return true;
        }

        if (correctPoolSize_)
        {
            return false;
        }
    }

    const float viewportHeight = static_cast<float>(layout_->size().y);
    if (viewportHeight <= 0.F)
    {
        return false; // we still don't know the correct value
    }

    int old_size = poolSize_;
    poolSize_ = std::min(std::max(1, static_cast<int>(std::floor(viewportHeight / kItemHeight))), items_size);
    correctPoolSize_ = true;
    return old_size != poolSize_;
}

int DirectoryColumn::computePageStep()
{
    computeViewportSlots(false);
    return poolSize_;
}

void DirectoryColumn::syncPool(bool size_change)
{
    const bool needs_redraw = computeViewportSlots(size_change);
    if (!needs_redraw)
        return; // viewport not laid out yet

    // The pool should have exactly as many items as fit on screen,
    // but never more than the total number of entries.
    const int needed = poolSize_;
    const int current = static_cast<int>(pool_.size());
    // in computeViewportSlots we checked that they are different

    const int maxStart = std::max(0, items_size - needed);
    visibleStart_ = std::clamp(visibleStart_, 0, maxStart);

    if (needed > current)
    {
        // Grow pool
        pool_.reserve(needed);
        for (int i = current; i < needed; ++i)
        {
            // add new elements in the newly allocated space
            const int entryIndex = visibleStart_ + i;
            const auto &path = entries_[entryIndex];
            pool_.push_back(makeShared<FileItemLayout>(backend_, path));
            pool_.back()->setSearchMatched(entryMatchesSearch(entryIndex));
            pool_.back()->setMultiSelected(isMultiSelectedIndex(entryIndex));
        }
    }
    else
    {
        // Shrink pool — remove excess items from fileList_ and pool_
        pool_.resize(needed);
    }

    rebuildFileList();
}

void DirectoryColumn::rebuildFileList()
{
    fileList_->clearChildren();
    for (auto &item : pool_)
        fileList_->addChild(item->getLayout());
}

void DirectoryColumn::updateVisibleRange(bool size_change)
{
    if (entries_.empty())
        return;

    syncPool(size_change);

    if (pool_.empty())
        return;

    const int poolSize = static_cast<int>(pool_.size());

    // Compute the new visibleStart_ so that selectedIndex_ is within the pool window.
    int newStart = visibleStart_;

    if (selectedIndex_ < visibleStart_)
    {
        // Selection scrolled above the visible window — snap window to selection
        newStart = selectedIndex_;
    }
    else if (selectedIndex_ >= visibleStart_ + poolSize)
    {
        // Selection scrolled below the visible window — move window down
        newStart = selectedIndex_ - poolSize + 1;
    }

    // Clamp to valid range
    const int maxStart = std::max(0, items_size - poolSize);
    newStart = std::clamp(newStart, 0, maxStart);

    const bool needsRebind = size_change || newStart != visibleStart_;
    if (newStart != visibleStart_)
        visibleStart_ = newStart;

    if (needsRebind)
    {
        const int poolIndex = selectedIndex_ - visibleStart_;

        for (int i = 0; i < poolSize; ++i)
        {
            const int entryIndex = visibleStart_ + i;
            if (entryIndex < items_size)
            {
                pool_[i]->rebind(entries_[entryIndex], i == poolIndex, isMultiSelectedIndex(entryIndex));
                pool_[i]->setSearchMatched(entryMatchesSearch(entryIndex));
            }
        }
    }
    else
    {
        // No window shift — just mark the selected pool item
        const int poolIndex = selectedIndex_ - visibleStart_;
        if (poolIndex >= 0 && poolIndex < poolSize)
            pool_[poolIndex]->setSelected(true);
        updateVisibleMultiSelection();
    }
}

WP<FileItem> DirectoryColumn::setSelection(int newIndex, bool requireFullscreenSupport)
{
    if (items_size <= 0)
        return WP<FileItem>{};

    if (newIndex < 0)
        newIndex = 0;
    if (newIndex >= items_size)
        newIndex = items_size - 1;

    if (requireFullscreenSupport)
    {
        // if the new selection doesn't support fullscreen, we don't change the selection and we return an empty pointer
        if (!entries_[newIndex]->fullscreenSupport())
        {
            return WP<FileItem>{};
        }
    }

    // Deselect old item (if it's in the pool)
    if (selectedIndex_ >= 0 && selectedIndex_ < items_size)
    {
        const int oldPoolIndex = selectedIndex_ - visibleStart_;
        if (oldPoolIndex >= 0 && oldPoolIndex < poolSize_)
            pool_[oldPoolIndex]->setSelected(false);
    }

    selectedIndex_ = newIndex;
    updateVisibleRange(false);

    const int poolIndex = selectedIndex_ - visibleStart_;
    if (poolIndex >= 0 && poolIndex < poolSize_)
        return pool_[poolIndex]->getItem();

    return WP<FileItem>{};
}

WP<FileItem> DirectoryColumn::setSelection(const std::filesystem::path &path)
{
    for (int i = 0; i < items_size; ++i)
    {
        if (entries_[i]->getPath() == path)
            return setSelection(i, false);
    }

    // Path not found -- fall back to first item
    return setSelection(0, false);
}

bool DirectoryColumn::hasMultiSelection() const
{
    return multiSelection_.has_value();
}

WP<FileItem> DirectoryColumn::startSelection()
{
    if (entries_.empty() || selectedIndex_ < 0 || selectedIndex_ >= items_size)
        return WP<FileItem>{};

    multiSelection_ = MultiSelectionState{selectedIndex_, entries_[selectedIndex_]->getPath()};
    updateVisibleMultiSelection();
    return getSelection();
}

WP<FileItem> DirectoryColumn::cancelMultiSelection()
{
    if (!multiSelection_)
        return getSelection();

    multiSelection_.reset();
    updateVisibleMultiSelection();
    return getSelection();
}

bool DirectoryColumn::isMultiSelectedIndex(int index) const
{
    if (!multiSelection_ || selectedIndex_ < 0 || index < 0 || index >= items_size)
        return false;

    const int first = std::min(multiSelection_->sourceIndex, selectedIndex_);
    const int last = std::max(multiSelection_->sourceIndex, selectedIndex_);
    return index >= first && index <= last;
}

std::vector<std::filesystem::path> DirectoryColumn::selectedOperationSources() const
{
    std::vector<std::filesystem::path> sources;
    if (entries_.empty() || selectedIndex_ < 0 || selectedIndex_ >= items_size)
        return sources;

    int first = selectedIndex_;
    int last = selectedIndex_;
    if (multiSelection_)
    {
        first = std::min(multiSelection_->sourceIndex, selectedIndex_);
        last = std::max(multiSelection_->sourceIndex, selectedIndex_);
    }

    sources.reserve(static_cast<std::size_t>(last - first + 1));
    for (int i = first; i <= last; ++i)
    {
        std::error_code ec;
        const auto source = std::filesystem::absolute(entries_[i]->getPath(), ec).lexically_normal();
        if (ec)
        {
            std::cerr << "[hyprfile] selection: failed to resolve absolute path for '" << entries_[i]->getPath()
                      << "': " << ec.message() << "\n";
            return {};
        }
        sources.push_back(source);
    }

    return sources;
}

void DirectoryColumn::updateVisibleMultiSelection()
{
    for (int i = 0; i < static_cast<int>(pool_.size()); ++i)
        pool_[i]->setMultiSelected(isMultiSelectedIndex(visibleStart_ + i));
}

void DirectoryColumn::reconcileMultiSelectionSource()
{
    if (!multiSelection_)
        return;

    for (int i = 0; i < items_size; ++i)
    {
        if (entries_[i]->getPath() == multiSelection_->sourcePath)
        {
            multiSelection_->sourceIndex = i;
            return;
        }
    }

    multiSelection_.reset();
}

std::optional<std::filesystem::path> DirectoryColumn::nextSelectionAfterOperation() const
{
    if (entries_.empty() || selectedIndex_ < 0 || selectedIndex_ >= items_size)
        return std::nullopt;

    int first = selectedIndex_;
    int last = selectedIndex_;
    if (multiSelection_)
    {
        first = std::min(multiSelection_->sourceIndex, selectedIndex_);
        last = std::max(multiSelection_->sourceIndex, selectedIndex_);
    }

    if (last + 1 < items_size)
        return entries_[last + 1]->getPath();
    if (first - 1 >= 0)
        return entries_[first - 1]->getPath();
    return std::nullopt;
}

WP<FileItem> DirectoryColumn::toggleHidden()
{
    if (empty_)
        return WP<FileItem>{};

    FileSystemService::get().toggleHiddenFiles();
    return refresh();
}

WP<FileItem> DirectoryColumn::refresh()
{
    if (empty_)
        return WP<FileItem>{};

    std::filesystem::path selectedPath;
    const std::filesystem::path* preferredSelection = nullptr;
    if (auto selection = getSelection().lock(); selection)
    {
        selectedPath = selection->getPath();
        preferredSelection = &selectedPath;
    }

    const int fallbackIndex = selectedIndex_ >= 0 ? selectedIndex_ : 0;
    return reloadEntries(preferredSelection, fallbackIndex);
}

void DirectoryColumn::setSearchQuery(std::string query)
{
    searchQuery_ = std::move(query);
    updateVisibleSearchMatches();
}

void DirectoryColumn::clearSearch()
{
    searchQuery_.clear();
    updateVisibleSearchMatches();
}

WP<FileItem> DirectoryColumn::commitSearch()
{
    return jumpToSearchResult(1, 1, "commit");
}

WP<FileItem> DirectoryColumn::jumpToNextSearchResult(int count)
{
    return jumpToSearchResult(count, 1, "next");
}

WP<FileItem> DirectoryColumn::jumpToPreviousSearchResult(int count)
{
    return jumpToSearchResult(count, -1, "previous");
}

bool DirectoryColumn::entryMatchesSearch(int index) const
{
    if (searchQuery_.empty() || index < 0 || index >= items_size)
        return false;

    const auto filename = lowercase(entries_[index]->getPath().filename().string());
    const auto query = lowercase(searchQuery_);
    return filename.find(query) != std::string::npos;
}

WP<FileItem> DirectoryColumn::jumpToSearchResult(int count, int direction, const char *actionName)
{
    if (searchQuery_.empty())
    {
        std::cerr << "[hyprfile] search " << actionName << ": search is not active\n";
        return getSelection();
    }

    if (entries_.empty() || selectedIndex_ < 0)
        return getSelection();

    count = std::max(1, count);
    int candidate = selectedIndex_;

    for (int step = 0; step < count; ++step)
    {
        bool found = false;
        for (int checked = 0; checked < items_size; ++checked)
        {
            candidate += direction;
            if (candidate < 0)
                candidate = items_size - 1;
            else if (candidate >= items_size)
                candidate = 0;

            if (entryMatchesSearch(candidate))
            {
                found = true;
                break;
            }
        }

        if (!found)
            return getSelection();
    }

    return setSelection(candidate, false);
}

void DirectoryColumn::updateVisibleSearchMatches()
{
    for (int i = 0; i < static_cast<int>(pool_.size()); ++i)
        pool_[i]->setSearchMatched(entryMatchesSearch(visibleStart_ + i));
}

void DirectoryColumn::renderEmptyState()
{
    fileList_->clearChildren();
    auto text = CTextBuilder::begin()
                    ->text("This folder is empty")
                    ->align(Hyprtoolkit::HT_FONT_ALIGN_CENTER)
                    ->color([this]()
                            { return backend_->getPalette()->m_colors.text; })
                    ->commence();
    fileList_->addChild(text);
}

void DirectoryColumn::openTerminal()
{
    TerminalLauncher::open(path_);
}

WP<FileItem> DirectoryColumn::copySelection()
{
    const auto sources = selectedOperationSources();
    if (sources.empty())
        return WP<FileItem>{};

    FileOperationClipboard clipboard;
    if (!clipboard.write(FileOperationClipboard::Operation::Copy, sources))
        std::cerr << "[hyprfile] copy: failed to store clipboard state\n";

    return cancelMultiSelection();
}

WP<FileItem> DirectoryColumn::cutSelection()
{
    const auto sources = selectedOperationSources();
    if (sources.empty())
        return WP<FileItem>{};

    FileOperationClipboard clipboard;
    if (!clipboard.write(FileOperationClipboard::Operation::Cut, sources))
        std::cerr << "[hyprfile] cut: failed to store clipboard state\n";

    return cancelMultiSelection();
}

DirectoryColumn::PasteSelectionResult DirectoryColumn::pasteSelection()
{
    if (empty_)
        return {};

    FileOperationClipboard clipboard;
    auto state = clipboard.read();
    if (!state)
        return {getSelection(), false};

    if (state->sources.empty())
    {
        std::cerr << "[hyprfile] paste: clipboard source list is empty\n";
        return {getSelection(), false};
    }

    auto result = FileOperations::paste(*state, path_);
    const bool shouldRefreshDirectory = refreshedDirectory(*state, result);
    if (result.success)
    {
        if (!clipboard.clear())
            std::cerr << "[hyprfile] paste: failed to clear clipboard after successful paste\n";

        if (shouldRefreshDirectory && !result.destinations.empty())
        {
            const auto preferredSelection = result.destinations.back();
            const int fallbackIndex = selectedIndex_ >= 0 ? selectedIndex_ : 0;
            return {reloadEntries(&preferredSelection, fallbackIndex), true};
        }

        return {getSelection(), false};
    }

    if (!result.error.empty())
        std::cerr << "[hyprfile] paste: " << result.error << "\n";

    if (shouldRefreshDirectory && !result.destinations.empty())
    {
        if (!result.remainingSources.empty())
        {
            if (!clipboard.write(state->operation, result.remainingSources))
            {
                std::cerr << "[hyprfile] paste: failed to update clipboard after partial paste\n";
                if (!clipboard.clear())
                    std::cerr << "[hyprfile] paste: failed to clear stale clipboard after partial paste\n";
            }
        }
        else if (!clipboard.clear())
        {
            std::cerr << "[hyprfile] paste: failed to clear clipboard after partial paste\n";
        }

        const auto preferredSelection = result.destinations.back();
        const int fallbackIndex = selectedIndex_ >= 0 ? selectedIndex_ : 0;
        return {reloadEntries(&preferredSelection, fallbackIndex), true};
    }

    return {getSelection(), false};
}

WP<FileItem> DirectoryColumn::trash()
{
    const auto sources = selectedOperationSources();
    if (sources.empty())
        return WP<FileItem>{};

    const auto preferredSelection = nextSelectionAfterOperation();
    const int fallbackIndex = multiSelection_ ? std::min(multiSelection_->sourceIndex, selectedIndex_) : selectedIndex_;

    for (const auto &source : sources)
    {
        if (!FileSystemService::trashWithGio(source))
        {
            std::cerr << "[hyprfile] trash: trashWithGio failed for '" << source << "'\n";
            multiSelection_.reset();
            return refresh();
        }
    }

    multiSelection_.reset();
    const std::filesystem::path *preferredSelectionPtr = preferredSelection ? &*preferredSelection : nullptr;
    return reloadEntries(preferredSelectionPtr, fallbackIndex);
}
