#pragma once

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/ScrollArea.hpp>
#include <hyprtoolkit/element/Text.hpp>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "../core/KeyBindings.hpp"

namespace hyprfile::UI
{
    using HelpEntries = std::vector<std::pair<std::string, std::string>>;

    struct HelpPopupLayout
    {
        Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CColumnLayoutElement> root;
        Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CScrollAreaElement> body;
        std::vector<Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CRowLayoutElement>> rows;
        Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CTextElement> footer;
    };

    HelpPopupLayout makeHelpPopupContent(Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> backend,
                                          const HelpEntries &helpEntries);

    std::string helpPopupIntroText();

    void bindHelpPopupCloseKeys(Hyprutils::Signal::CSignalT<Hyprtoolkit::Input::SKeyboardKeyEvent> &keyboardKey,
                                const CKeyBindings &keyBindings,
                                std::function<void()> closePopup);
}
