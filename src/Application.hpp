// Application class definition
#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/core/Timer.hpp>
#include <hyprtoolkit/window/Window.hpp>
#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/Text.hpp>
#include <hyprtoolkit/element/Image.hpp>
#include <hyprtoolkit/element/Button.hpp>
#include <hyprtoolkit/element/Null.hpp>

#include <iostream>

#include "cli/Parser.hpp"
#include "core/KeyBindings.hpp"

#include "ui/TopBar.hpp"
#include "ui/StatusBar.hpp"
#include "ui/columns/ParentColumn.hpp"
#include "ui/columns/CurrentColumn.hpp"
#include "ui/columns/PreviewColumn.hpp"
#include "ui/columns/layout/PreviewVideoColumn.hpp"

using namespace Hyprutils::Memory;
using namespace Hyprutils::Math;
using namespace Hyprtoolkit;
using namespace std;

class Application
{
public:
    explicit Application(const CLI::SCLIOptions *opts);

    int run();

    SP<CColumnLayoutElement> makeRectangle();
    void showHelpPopup();
    void hidePopup();

private:
    const SP<IBackend> backend_;
    const bool verbose_;

    CKeyBindings keyBindings;

    bool helpActive_ = false;
    SP<IWindow> helpWindow_;
    WP<IWindow> window_;
    SP<CRowLayoutElement> main_layout_;
    ParentColumn parentColumn_;
    CurrentColumn currentColumn_;
    PreviewColumn previewColumn_;
    TopBar topBar_;
    StatusBar statusBar_;

    void setUpKeyBindings();
    void changePreview(WP<FileItem> &item, bool forceRebuild = false);
    void updateStatusBarDirectoryCount();

    void enterFullscreen();
    void restoreMainLayout();
    void handleQuitOrCancel();

    void run_key_binding_action(const CKeyAction *action, int count)
    {
        cout << "Running action: " << action->getDescription() << " (count=" << count << ")\n";
        (*action)(count);
    }

    // Count prefix state (vim-style number prefix for navigation)
    int countPrefix_ = 0;
    static constexpr int MAX_COUNT_PREFIX = 9999;
    int consumeCount();

    // Search input state
    bool searchInputActive_ = false;
    std::string searchBuffer_;
    void beginSearchInput();
    bool handleSearchInput(Hyprtoolkit::Input::SKeyboardKeyEvent e, uint32_t normalizedMask);
    void updateLiveSearch();
    void cancelSearchInput();
    void commitSearchInput();

    // Key repeat state
    static constexpr auto REPEAT_INITIAL_DELAY = std::chrono::milliseconds(300);
    static constexpr auto REPEAT_INTERVAL = std::chrono::milliseconds(50);

    uint32_t repeatGeneration_ = 0;
    CKeyAction *heldAction_ = nullptr;
    Hyprutils::Memory::CAtomicSharedPointer<Hyprtoolkit::CTimer> repeatTimer_;

    bool isRepeatableAction(const CKeyAction *action) const;
    void cancelRepeat();
    void startRepeat(CKeyAction *action);
    void scheduleRepeatTick(uint32_t generation);
};
