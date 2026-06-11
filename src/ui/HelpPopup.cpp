#include "HelpPopup.hpp"

#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/types/FontTypes.hpp>
#include <hyprtoolkit/types/SizeType.hpp>

namespace
{
    using namespace Hyprtoolkit;
    using Hyprutils::Memory::CSharedPointer;

    constexpr float kHelpBodyHeightPercent = 0.72F;

    CSharedPointer<CTextElement> makeHelpText(CSharedPointer<IBackend> backend, std::string text,
                                              CFontSize fontSize = {CFontSize::HT_FONT_TEXT},
                                              eFontAlignment align = HT_FONT_ALIGN_CENTER)
    {
        return CTextBuilder::begin()
            ->text(std::move(text))
            ->fontSize(std::move(fontSize))
            ->align(align)
            ->color([backend]
                    { return backend->getPalette()->m_colors.text; })
            ->commence();
    }

    std::string formatHelpRow(const std::string &keys, const std::string &description)
    {
        return "  " + keys + "  -  " + description;
    }

    CSharedPointer<CRowLayoutElement> makeHelpRow(CSharedPointer<IBackend> backend, std::string text)
    {
        auto row = CRowLayoutBuilder::begin()
                       ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                       ->commence();

        row->addChild(makeHelpText(std::move(backend), std::move(text), {CFontSize::HT_FONT_TEXT}, HT_FONT_ALIGN_LEFT));
        return row;
    }

    const CKeyAction *findActionForEvent(const CKeyBindings &keyBindings,
                                         const Hyprtoolkit::Input::SKeyboardKeyEvent &event)
    {
        const uint32_t normalizedMask = normalizeModifierMask(event.modMask);
        const auto &map = keyBindings.getBindingsMap();

        if (auto it = map.find({event.xkbKeysym, normalizedMask}); it != map.end())
            return it->second;

        if (auto noMod = map.find({event.xkbKeysym, 0}); noMod != map.end())
            return noMod->second;

        return nullptr;
    }
}

namespace hyprfile::UI
{
    std::string helpPopupIntroText()
    {
        return "Keyboard-only file manager";
    }

    HelpPopupLayout makeHelpPopupContent(CSharedPointer<IBackend> backend, const HelpEntries &helpEntries)
    {
        HelpPopupLayout layout;

        layout.root = CColumnLayoutBuilder::begin()
                          ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, 1.F}})
                          ->gap(6)
                          ->commence();

        auto title = makeHelpText(backend, "hyprfile Help", {CFontSize::HT_FONT_H2});

        auto line = CRectangleBuilder::begin()
                        ->color([backend]
                                { return CHyprColor{backend->getPalette()->m_colors.text.darken(0.5)}; })
                        ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_ABSOLUTE, {1.F, 2.F}})
                        ->commence();

        auto intro = makeHelpText(backend, helpPopupIntroText());
        layout.footer = makeHelpText(backend, "Press Esc to close help.");

        layout.body = CScrollAreaBuilder::begin()
                          ->scrollY(true)
                          ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_PERCENT, {1.F, kHelpBodyHeightPercent}})
                          ->commence();

        auto bodyColumn = CColumnLayoutBuilder::begin()
                              ->gap(2)
                              ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                              ->commence();

        for (const auto &[keys, description] : helpEntries)
        {
            auto row = makeHelpRow(backend, formatHelpRow(keys, description));
            bodyColumn->addChild(row);
            layout.rows.push_back(std::move(row));
        }

        layout.body->addChild(bodyColumn);

        layout.root->addChild(title);
        layout.root->addChild(line);
        layout.root->addChild(intro);
        layout.root->addChild(layout.body);
        layout.root->addChild(layout.footer);

        return layout;
    }

    void bindHelpPopupCloseKeys(Hyprutils::Signal::CSignalT<Hyprtoolkit::Input::SKeyboardKeyEvent> &keyboardKey,
                                const CKeyBindings &keyBindings,
                                std::function<void()> closePopup)
    {
        keyboardKey.listenStatic([&keyBindings, closePopup = std::move(closePopup)](Hyprtoolkit::Input::SKeyboardKeyEvent event)
                                 {
            if (!event.down || event.repeat)
                return;

            const CKeyAction *action = findActionForEvent(keyBindings, event);
            if (action != &keyBindings.quit && action != &keyBindings.showHelp)
                return;

            if (closePopup)
                closePopup(); });
    }
}
