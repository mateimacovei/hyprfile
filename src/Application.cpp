#include "Application.hpp"

#include <algorithm>
#include <hyprutils/math/Vector2D.hpp>
#include <xkbcommon/xkbcommon.h>

#include "core/FileLauncher.hpp"
#include "core/ProcessLauncher.hpp"
#include "core/SearchInput.hpp"
#include "ui/ApplicationLayout.hpp"
#include "ui/HelpPopup.hpp"

SP<IBackend> createBackend(bool verbose)
{
    auto loggerPtr = Hyprutils::Memory::makeShared<Hyprutils::CLI::CLogger>();
    if (verbose)
    {
        loggerPtr->setEnableStdout(true);
        loggerPtr->setLogLevel(Hyprutils::CLI::LOG_DEBUG);
    }
    else
    {
        loggerPtr->setEnableStdout(false);
        loggerPtr->setLogLevel(Hyprutils::CLI::LOG_WARN);
    }
    Hyprtoolkit::IBackend::SBackendCreationData backendData;

    auto loggerConn = Hyprutils::Memory::makeShared<Hyprutils::CLI::CLoggerConnection>(*loggerPtr);
    loggerConn->setLogLevel(verbose ? Hyprutils::CLI::LOG_DEBUG : Hyprutils::CLI::LOG_WARN);
    backendData.pLogConnection = loggerConn;
    return IBackend::createWithData(backendData);
}

Application::Application(const CLI::SCLIOptions *opts)
    : verbose_(opts->verbose), backend_(createBackend(opts->verbose)),
      parentColumn_(backend_, 0.16F),
      currentColumn_(backend_, 0.4F),
      previewColumn_(backend_, 0.4F),
      topBar_(backend_, 0),
      statusBar_(backend_)
{
    auto cwd = opts->cwd;
    cout << "Starting application in " << cwd.string() << '\n';

    auto parent_layout = parentColumn_.instantiateParentColumn(cwd);
    parentColumn_.set_layout_column(parent_layout);
    parentColumn_.draw();
    parentColumn_.setSelection(cwd);

    auto current_layout = currentColumn_.instantiateColumn(cwd);
    currentColumn_.set_layout_column(current_layout);
    currentColumn_.draw();
    auto preview_item = currentColumn_.get_layout_column()->getSelection();

    if (preview_item)
    {
        previewColumn_.set_layout_column(previewColumn_.createPreview(preview_item.lock()));
        previewColumn_.draw();
    }
}

int Application::run()
{

    auto window = CWindowBuilder::begin()->minSize({680, 380})->appTitle("hyprfile")->appClass("hyprfile")->commence();

    window_ = window;

    window->m_rootElement->addChild(CRectangleBuilder::begin()->color([this]
                                                                      { return this->backend_->getPalette()->m_colors.background; })
                                        ->commence());
    auto application_layout = CColumnLayoutBuilder::begin()->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})->commence();
    application_layout->setMargin(2);
    window->m_rootElement->addChild(application_layout);

    topBar_.updatePath(currentColumn_.get_layout_column()->getPath());
    updateStatusBarDirectoryCount();

    auto main_layout = hyprfile::UI::makeMainLayout();
    main_layout->setGrow(true);

    application_layout->addChild(topBar_.getLayout());
    application_layout->addChild(main_layout);
    application_layout->addChild(statusBar_.getLayout());

    main_layout->addChild(parentColumn_.getLayout());
    main_layout->addChild(makeRectangle());

    main_layout->addChild(currentColumn_.getLayout());
    main_layout->addChild(makeRectangle());

    auto preview_layout = previewColumn_.getLayout();
    if (preview_layout)
    {
        main_layout->addChild(preview_layout);
    }

    // the preview layout will be added later
    main_layout_ = main_layout; // store a reference to the main layout so we can add the preview column later when we have something to preview

    setUpKeyBindings();

    window->m_events.resized.listenStatic([this](Hyprutils::Math::Vector2D)
                                          {
                                              parentColumn_.resync();
                                              currentColumn_.resync();
                                              previewColumn_.resync(); });

    window->open();

    this->backend_->enterLoop();

    cout << "Exited application loop, shutting down...\n";

    return 0;
}

void Application::changePreview(WP<FileItem> &item, bool forceRebuild)
{
    updateStatusBarDirectoryCount();

    SP<FileItem> spItem = item.lock();
    if (!spItem)
    {
        if (forceRebuild)
        {
            statusBar_.clearPermChars();
            if (previewColumn_.isFullscreen())
            {
                restoreMainLayout();
                return;
            }

            if (const SP<BaseLayoutColumn> old_preview = previewColumn_.get_layout_column(); old_preview)
            {
                main_layout_->removeChild(old_preview->getLayout());
                previewColumn_.set_layout_column(nullptr);
            }
        }
        return;
    }

    const SP<BaseLayoutColumn> old_preview = previewColumn_.get_layout_column();

    if (!forceRebuild && old_preview && spItem->getPath() == old_preview->getPath())
    {
        return;
    }

    statusBar_.update(item);

    if (previewColumn_.isFullscreen())
    {
        // In fullscreen mode: create a full-width preview (if possible) and swap it

        if (!spItem->fullscreenSupport())
        {
            restoreMainLayout();
            return;
        }
        SP<BaseLayoutColumn> new_preview = previewColumn_.createFullscreenPreview(spItem);
        if (new_preview)
        {
            new_preview->draw();
            main_layout_->clearChildren();

            previewColumn_.set_layout_column(new_preview);
            main_layout_->addChild(new_preview->getLayout());
            const auto currentLayout = currentColumn_.get_layout_column();
            const auto directoryPath = currentLayout ? currentLayout->getPath() : spItem->getPath().parent_path();
            topBar_.updatePath(topBarDisplayPath(directoryPath, spItem->getPath(), true));
        }
        return;
    }

    // Normal mode
    SP<BaseLayoutColumn> new_preview = previewColumn_.createPreview(spItem);
    if (new_preview)
    {
        new_preview->draw();
        if (old_preview)
            main_layout_->removeChild(old_preview->getLayout());

        previewColumn_.set_layout_column(new_preview);
        main_layout_->addChild(new_preview->getLayout());
    }
}

void Application::updateStatusBarDirectoryCount()
{
    const auto currentLayout = currentColumn_.get_layout_directory_column();
    statusBar_.setDirectoryItemCount(currentLayout ? currentLayout->getTotalItemsCount() : 0);
}

void Application::enterFullscreen()
{
    if (previewColumn_.isFullscreen())
        return;

    SP<BaseLayoutColumn> current_layout = currentColumn_.get_layout_directory_column();
    if (!current_layout)
        return;

    SP<FileItem> selection = current_layout->getSelection().lock();
    if (!selection)
        return;

    SP<BaseLayoutColumn> fullscreen_preview = previewColumn_.createFullscreenPreview(selection);
    if (!fullscreen_preview)
        return;

    fullscreen_preview->draw();

    main_layout_->clearChildren();
    previewColumn_.set_layout_column(fullscreen_preview);
    main_layout_->addChild(fullscreen_preview->getLayout());
    topBar_.updatePath(topBarDisplayPath(current_layout->getPath(), selection->getPath(), true));
}

void Application::restoreMainLayout()
{
    // Re-create the preview at normal width based on current selection
    SP<BaseLayoutColumn> current_layout = currentColumn_.get_layout_directory_column();
    SP<BaseLayoutColumn> new_preview = nullptr;

    if (current_layout)
    {
        SP<FileItem> selection = current_layout->getSelection().lock();
        if (selection)
        {
            new_preview = previewColumn_.createPreview(selection);
            if (new_preview)
                new_preview->draw();
        }
    }

    previewColumn_.set_layout_column(new_preview);
    if (current_layout)
        topBar_.updatePath(topBarDisplayPath(current_layout->getPath(), {}, false));

    main_layout_->clearChildren();
    main_layout_->addChild(parentColumn_.getLayout());
    main_layout_->addChild(makeRectangle());
    main_layout_->addChild(currentColumn_.getLayout());
    main_layout_->addChild(makeRectangle());
    if (new_preview)
        main_layout_->addChild(new_preview->getLayout());
}

void Application::beginSearchInput()
{
    cancelRepeat();
    countPrefix_ = 0;
    statusBar_.setCount(0);
    searchInputActive_ = true;
    searchBuffer_.clear();
    updateLiveSearch();
}

void Application::updateLiveSearch()
{
    statusBar_.setSearchQuery(searchBuffer_);
    currentColumn_.setSearchQuery(searchBuffer_);
}

void Application::cancelSearchInput()
{
    searchInputActive_ = false;
    searchBuffer_.clear();
    statusBar_.clearSearchQuery();
    currentColumn_.clearSearch();
}

void Application::commitSearchInput()
{
    searchInputActive_ = false;
    statusBar_.clearSearchQuery();
    currentColumn_.setSearchQuery(searchBuffer_);
    auto newItem = currentColumn_.commitSearch();
    changePreview(newItem);
}

void Application::handleQuitOrCancel()
{
    if (searchInputActive_)
    {
        cancelSearchInput();
        return;
    }

    if (helpActive_)
    {
        hidePopup();
        return;
    }

    if (previewColumn_.isFullscreen())
    {
        restoreMainLayout();
        return;
    }

    if (currentColumn_.hasMultiSelection())
    {
        auto selection = currentColumn_.cancelMultiSelection();
        changePreview(selection);
        return;
    }

    window_->close();
    backend_->destroy();
}

bool Application::handleSearchInput(Hyprtoolkit::Input::SKeyboardKeyEvent e, uint32_t normalizedMask)
{
    if (!searchInputActive_)
        return false;

    if (SearchInput::isCancelKeysym(e.xkbKeysym))
    {
        handleQuitOrCancel();
        return true;
    }

    if (e.xkbKeysym == XKB_KEY_Return || e.xkbKeysym == XKB_KEY_KP_Enter)
    {
        commitSearchInput();
        return true;
    }

    if (e.xkbKeysym == XKB_KEY_BackSpace)
    {
        SearchInput::popLastUtf8Codepoint(searchBuffer_);
        updateLiveSearch();
        return true;
    }

    if (SearchInput::appendKeysym(searchBuffer_, e.xkbKeysym, normalizedMask))
    {
        updateLiveSearch();
    }

    return true;
}

void Application::setUpKeyBindings()
{
    keyBindings.moveDown.setCountAction([this](int count)
                                        { auto new_item = currentColumn_.moveDown( count, previewColumn_.isFullscreen() ); 
                                 changePreview(new_item); });
    keyBindings.moveUp.setCountAction([this](int count)
                                      { auto new_item = currentColumn_.moveUp( count, previewColumn_.isFullscreen() ); 
                                 changePreview(new_item); });
    keyBindings.goToParent.setAction([this]()
                                     {
                                        if(previewColumn_.isFullscreen())
                                        {
                                            auto new_item = currentColumn_.moveUp( 1, previewColumn_.isFullscreen() ); 
                                            changePreview(new_item);
                                            return;
                                        }

                                        // for later: maybe this can be improved by switching hte layout columns between the 3 main columns, instead of recreating them
                                         auto existing_parent_layout = parentColumn_.get_layout_directory_column();
                                         if (!existing_parent_layout || existing_parent_layout->empty())
                                             return;

                                         auto existing_current_layout = currentColumn_.get_layout_directory_column();
                                         if (!existing_current_layout)
                                             return;

                                         auto existing_parent_path = existing_parent_layout->getPath();
                                         // in currrent column I have the children of the current column path
                                         auto existing_current_path = existing_current_layout->getPath();
                                         std::optional<std::filesystem::path> existing_selection_path ;
                                         auto existing_selection_path_ref= existing_current_layout->getSelection().lock();
                                         if (existing_selection_path_ref){
                                            existing_selection_path = existing_selection_path_ref->getPath();
                                         } 

                                         auto new_parent_layout = parentColumn_.instantiateParentColumn(existing_parent_path);
                                         auto new_current_layout = currentColumn_.instantiateColumn(existing_parent_path);
                                         auto new_preview_layout = currentColumn_.instantiateColumn(existing_current_path);

                                         parentColumn_.set_layout_column(new_parent_layout);
                                         currentColumn_.set_layout_column(new_current_layout);
                                         previewColumn_.set_layout_column(new_preview_layout);

                                         main_layout_->clearChildren();
                                         main_layout_->addChild(new_parent_layout->getLayout());
                                         main_layout_->addChild(makeRectangle());
                                         main_layout_->addChild(new_current_layout->getLayout());
                                         main_layout_->addChild(makeRectangle());
                                         main_layout_->addChild(new_preview_layout->getLayout());

                                         parentColumn_.draw();
                                         parentColumn_.setSelection(existing_parent_path);
                                         currentColumn_.draw();
                                         currentColumn_.setSelection(existing_current_path);
                                         previewColumn_.draw();
                                         if(existing_selection_path){
                                            previewColumn_.setSelection(existing_selection_path.value()); 
                                         }

                                          topBar_.updatePath(existing_parent_path);
                                          updateStatusBarDirectoryCount();
                                          auto parentSel = currentColumn_.get_layout_directory_column()->getSelection();
                                          statusBar_.update(parentSel); });

    std::function<void(FileItem::FileItemType, SP<FileItem>, SP<BaseLayoutColumn>)> goToChildAction = [this](FileItem::FileItemType previewType, SP<FileItem> preview_selection, SP<BaseLayoutColumn> preview_layout)
    {
        if (previewType == FileItem::FileItemType::Directory)
        {

            auto preview_path = preview_selection->getPath();
            auto new_parent_layout = parentColumn_.instantiateParentColumn(preview_path);
            auto new_current_layout = currentColumn_.instantiateColumn(preview_path);
            // we only have a preview if the old preview column had an item selected
            SP<BaseLayoutColumn> new_preview_layout;
            auto current_preview_selection = preview_layout->getSelection().lock();
            if (current_preview_selection)
            {
                new_preview_layout = previewColumn_.createPreview(current_preview_selection);
            }
            else
            {
                new_preview_layout = nullptr;
            }

            parentColumn_.set_layout_column(new_parent_layout);
            currentColumn_.set_layout_column(new_current_layout);
            previewColumn_.set_layout_column(new_preview_layout);

            main_layout_->clearChildren();
            main_layout_->addChild(new_parent_layout->getLayout());
            main_layout_->addChild(makeRectangle());
            main_layout_->addChild(new_current_layout->getLayout());
            main_layout_->addChild(makeRectangle());
            if (new_preview_layout)
            {
                main_layout_->addChild(new_preview_layout->getLayout());
            }

            parentColumn_.draw();
            parentColumn_.setSelection(preview_path);
            currentColumn_.draw();
            if (current_preview_selection)
            {
                currentColumn_.setSelection(current_preview_selection->getPath());
                previewColumn_.draw();
            }

            topBar_.updatePath(preview_path);
            updateStatusBarDirectoryCount();
            auto parentSel = currentColumn_.get_layout_directory_column()->getSelection();
            if (current_preview_selection)
            {
                statusBar_.update(current_preview_selection);
            }
            else
            {
                statusBar_.clearPermChars();
            }
        }
        else
        {
            // if it's not a directory, then we should check if it has a fullscreen preview, and if it does, open it in fullscreen, otherwise do nothing for now
            if ((previewType == FileItem::FileItemType::Image ||
                 previewType == FileItem::FileItemType::Video))
            {
                if (!previewColumn_.isFullscreen())
                {
                    enterFullscreen();
                }
                else
                {
                    preview_layout->searchRight();
                }
                return;
            }
        }
    };
    keyBindings.goToChild.setAction([this, goToChildAction]()
                                    {
                                        if (previewColumn_.isFullscreen())
                                        {
                                            auto new_item = currentColumn_.moveDown(1, previewColumn_.isFullscreen());
                                            changePreview(new_item);
                                            return;
                                        }

                                        SP<BaseLayoutColumn> current_layout = currentColumn_.get_layout_directory_column();
                                        SP<BaseLayoutColumn> preview_layout = previewColumn_.get_layout_column();
                                        SP<FileItem> preview_selection = current_layout->getSelection().lock();

                                        if (!preview_layout || !preview_selection)
                                            return;
                                        // preview_layout could be eigher a directory preview or a file preview

                                        auto previewType = preview_selection->getPreviewType(); 
                                        goToChildAction(previewType, preview_selection, preview_layout); });

    keyBindings.openSelection.setAction([this, goToChildAction]()
                                        {
                                            SP<BaseLayoutColumn> current_layout = currentColumn_.get_layout_directory_column();
                                            SP<BaseLayoutColumn> preview_layout = previewColumn_.get_layout_column();
                                            if (!current_layout || !preview_layout)
                                                return;

                                            SP<FileItem> selection = current_layout->getSelection().lock();
                                            if (!selection)
                                                return;

                                            const auto type = selection->getPreviewType();
                                            if (type == FileItem::FileItemType::Directory)
                                            {
                                                goToChildAction(type, selection, preview_layout);
                                                return;
                                            }

                                             currentColumn_.cancelMultiSelection();
                                             FileLauncher::open(selection->getPath(), type); });
    keyBindings.goToTop.setAction([this]()
                                  { auto new_item = currentColumn_.goToTop(previewColumn_.isFullscreen()); 
                                changePreview(new_item); });
    keyBindings.goToBottom.setAction([this]()
                                     { auto new_item = currentColumn_.goToBottom(previewColumn_.isFullscreen()); 
                                changePreview(new_item); });
    keyBindings.pageDown.setAction([this]()
                                   { auto new_item = currentColumn_.pageDown(previewColumn_.isFullscreen()); 
                                changePreview(new_item); });
    keyBindings.pageUp.setAction([this]()
                                 { auto new_item = currentColumn_.pageUp(previewColumn_.isFullscreen()); 
                                changePreview(new_item); });
    keyBindings.toggleHidden.setAction([this]()
                                       { auto new_item = currentColumn_.toggleHidden();

                                         auto parent_layout = parentColumn_.get_layout_directory_column();
                                         auto current_layout = currentColumn_.get_layout_directory_column();
                                         if (parent_layout && !parent_layout->empty())
                                         {
                                             parent_layout->refresh();
                                             if (current_layout)
                                                 parent_layout->setSelection(current_layout->getPath());
                                         }

                                changePreview(new_item, true); });
    keyBindings.refresh.setAction([this]()
                                  { auto new_item = currentColumn_.refresh(); 
                                changePreview(new_item, true); });
    keyBindings.trash.setAction([this]()
                                { auto new_item = currentColumn_.trash(); 
                                changePreview(new_item); });
    keyBindings.search.setAction([this]()
                                 { beginSearchInput(); });
    keyBindings.nextSearchResult.setCountAction([this](int count)
                                            { auto new_item = currentColumn_.jumpToNextSearchResult(count);
                                changePreview(new_item); });
    keyBindings.previousSearchResult.setCountAction([this](int count)
                                                { auto new_item = currentColumn_.jumpToPreviousSearchResult(count);
                                changePreview(new_item); });
    keyBindings.startSelection.setAction([this]()
                                         { auto new_item = currentColumn_.startSelection();
                                changePreview(new_item); });
    keyBindings.openTerminal.setAction([this]()
                                       { currentColumn_.openTerminal(); });
    keyBindings.openNewWindow.setAction([this]()
                                        {
                                            const auto current_layout = currentColumn_.get_layout_column();
                                            if (!current_layout)
                                                return;

                                            const auto cwd = current_layout->getPath();
                                            ProcessLauncher::spawnDetached({"/proc/self/exe", cwd.string()}, cwd); });
    keyBindings.copySelection.setAction([this]()
                                         { currentColumn_.copySelection(); });
    keyBindings.cutSelection.setAction([this]()
                                       { currentColumn_.cutSelection(); });
    keyBindings.pasteSelection.setAction([this]()
                                         { auto result = currentColumn_.pasteSelection();
                                if (result.refreshedDirectory)
                                    changePreview(result.selection, true); });
    keyBindings.quit.setAction([this]()
                               { handleQuitOrCancel(); });
    keyBindings.showHelp.setAction([this]()
                                   {
        if (helpActive_)
            hidePopup();
        else
            showHelpPopup(); });

    window_->m_events.closeRequest.listenStatic([this]
                                                {
        cout << "Close requested, exiting...\n";
        window_->close();
        this->backend_->destroy(); });

    window_->m_events.keyboardKey.listenStatic([this](Hyprtoolkit::Input::SKeyboardKeyEvent e)
                                               {
        // Key-up: cancel any active repeat.
        // Note: on Wayland, key-up events often have xkbKeysym=0 (the key state
        // is already updated before the release event), so we can't match by keysym.
        // Cancelling unconditionally is safe — if a different key was released,
        // the worst case is a harmless no-op (heldAction_ is already null).
        if (!e.down)
        {
            cancelRepeat();
            return;
        }

        const uint32_t normalizedMask = normalizeModifierMask(e.modMask);
        cout << "event: keysym=" << e.xkbKeysym << " mod=" << e.modMask << " normalized=" << normalizedMask << "\n";

        if (handleSearchInput(e, normalizedMask))
            return;

        // Ignore OS-level repeat events — we handle repeat ourselves via timers.
        // Search input is handled first so held printable keys and Backspace keep editing the query.
        if (e.repeat)
            return;

        // Intercept digit keys (0-9) with no modifiers to build count prefix
        if (normalizedMask == 0 && e.xkbKeysym >= XKB_KEY_0 && e.xkbKeysym <= XKB_KEY_9)
        {
            const int digit = e.xkbKeysym - XKB_KEY_0;
            // Don't start a count with 0 (it has no meaning as a leading digit)
            if (digit == 0 && countPrefix_ == 0)
            {
                // Fall through to normal key handling (0 is unbound anyway)
            }
            else
            {
                countPrefix_ = countPrefix_ * 10 + digit;
                if (countPrefix_ > MAX_COUNT_PREFIX)
                    countPrefix_ = MAX_COUNT_PREFIX;
                statusBar_.setCount(countPrefix_);
                return;
            }
        }

        // Look up the action for this key combo
        CKeyAction* action = nullptr;
        auto map = keyBindings.getBindingsMap();
        auto it = map.find({e.xkbKeysym, normalizedMask});
        if (it != map.end())
        {
            action = it->second;
        }
        else
        {
            auto no_mod = map.find({e.xkbKeysym, 0});
            if (no_mod != map.end())
                action = no_mod->second;
        }

        if (!action)
        {
            cout << "no binding\n";
            // Clear any pending count prefix on unbound key
            if (countPrefix_ > 0)
            {
                countPrefix_ = 0;
                statusBar_.setCount(0);
            }
            return;
        }

        // Consume count prefix
        int count = consumeCount();

        // Cancel any previous repeat (e.g. switching from up to down without releasing)
        cancelRepeat();

        // Fire the action immediately with the count
        run_key_binding_action(action, count);

        // Start key repeat if this is a repeatable action
        if (isRepeatableAction(action))
            startRepeat(action); });
}

SP<CColumnLayoutElement> Application::makeRectangle()
{

    auto layout = CColumnLayoutBuilder::begin()->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {0.02F, 1.F}})->commence();
    auto rectangle = CRectangleBuilder::begin()
                         ->color([this]
                                 { return CHyprColor{this->backend_->getPalette()->m_colors.text.darken(0.65)}; })
                         ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_PERCENT, {9.F, 0.99F}})
                         ->commence();

    rectangle->setMargin(4);
    layout->addChild(rectangle);
    return layout;
}

void Application::showHelpPopup()
{
    if (helpActive_)
        return;

    auto w = window_.lock();
    if (!w)
        return;

    helpActive_ = true;

    const Hyprutils::Math::Vector2D helpPopupSize{620.F, 540.F};
    const auto parentSize = w->pixelSize();
    const double parentScale = std::max(1.0, static_cast<double>(w->scale()));
    const Hyprutils::Math::Vector2D parentLogical{parentSize.x / parentScale, parentSize.y / parentScale};
    const auto parentCenter = parentLogical / 2.0;
    const auto popupHalf = helpPopupSize / 2.0;
    const Hyprutils::Math::Vector2D helpPosition{std::max(0.0, parentCenter.x - popupHalf.x),
                                                 std::max(0.0, parentCenter.y - popupHalf.y)};

    auto helpWindow = CWindowBuilder::begin()
                          ->type(HT_WINDOW_POPUP)
                          ->parent(w)
                          ->pos(helpPosition)
                          ->preferredSize(helpPopupSize)
                          ->minSize({560.F, 480.F})
                          ->commence();

    auto rootLayout = CColumnLayoutBuilder::begin()
                          ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                          ->commence();
    rootLayout->setMargin(10);
    rootLayout->setGrow(true);

    auto frame = CRectangleBuilder::begin()
                     ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                     ->color([]
                             { return CHyprColor(0.15, 0.15, 0.15, 1.0); })
                     ->borderColor([]
                                   { return CHyprColor(0.0, 0.5, 1.0, 1.0); })
                     ->borderThickness(2)
                     ->rounding(12)
                     ->commence();

    auto helpContent = hyprfile::UI::makeHelpPopupContent(backend_, keyBindings.getHelp());
    frame->addChild(helpContent.root);
    rootLayout->addChild(frame);
    helpWindow->m_rootElement->addChild(rootLayout);

    helpWindow_ = helpWindow;
    helpWindow_->m_events.closeRequest.listenStatic([this]
                                                    { helpActive_ = false; helpWindow_.reset(); });
    hyprfile::UI::bindHelpPopupCloseKeys(helpWindow_->m_events.keyboardKey, keyBindings, [this]
                                          { hidePopup(); });
    helpWindow_->open();
}

void Application::hidePopup()
{
    if (!helpActive_)
        return;

    helpActive_ = false;
    if (helpWindow_)
    {
        helpWindow_->close();
        helpWindow_.reset();
    }
}

bool Application::isRepeatableAction(const CKeyAction *action) const
{
    return action && action->isRepeatable();
}

void Application::cancelRepeat()
{
    ++repeatGeneration_; // Invalidate any in-flight timer callbacks
    if (repeatTimer_)
    {
        repeatTimer_->cancel();
        repeatTimer_.reset();
    }
    heldAction_ = nullptr;
}

void Application::scheduleRepeatTick(uint32_t generation)
{
    repeatTimer_ = backend_->addTimer(
        REPEAT_INTERVAL,
        [this, generation](Hyprutils::Memory::CAtomicSharedPointer<Hyprtoolkit::CTimer> /*self*/, void * /*data*/)
        {
            if (generation != repeatGeneration_ || !heldAction_)
                return;

            run_key_binding_action(heldAction_, 1);
            scheduleRepeatTick(generation);
        },
        nullptr);
}

void Application::startRepeat(CKeyAction *action)
{
    heldAction_ = action;
    uint32_t generation = ++repeatGeneration_;

    repeatTimer_ = backend_->addTimer(
        REPEAT_INITIAL_DELAY,
        [this, generation](Hyprutils::Memory::CAtomicSharedPointer<Hyprtoolkit::CTimer> /*self*/, void * /*data*/)
        {
            if (generation != repeatGeneration_ || !heldAction_)
                return;

            run_key_binding_action(heldAction_, 1);
            scheduleRepeatTick(generation);
        },
        nullptr);
}

int Application::consumeCount()
{
    int count = countPrefix_ > 0 ? countPrefix_ : 1;
    countPrefix_ = 0;
    statusBar_.setCount(0);
    return count;
}
