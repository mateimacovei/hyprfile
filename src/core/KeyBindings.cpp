#include "KeyBindings.hpp"

#include <hyprtoolkit/core/Input.hpp>
#include <xkbcommon/xkbcommon.h>
#include <sstream>

using namespace Hyprtoolkit::Input;

CKeyAction& CKeyAction::setDescription(std::string description) {
    description_ = std::move(description);
    return *this;
}

CKeyAction& CKeyAction::addBinding(uint32_t keysym, uint32_t mods) {
    bindings_.push_back(SKeyBinding(keysym, mods));
    return *this;
}

CKeyAction& CKeyAction::setRepeatable(bool repeatable) {
    repeatable_ = repeatable;
    return *this;
}

std::string CKeyAction::getKeyText() const
{
    std::ostringstream oss;
    for (size_t i = 0; i < bindings_.size(); ++i)
    {
        if (i > 0)
            oss << ", ";

        uint32_t keysym = bindings_[i].xkbKeysym;
        uint32_t mods = bindings_[i].modMask;

        bool hasShift = mods & Hyprtoolkit::Input::HT_MODIFIER_SHIFT;
        bool hasCtrl = mods & Hyprtoolkit::Input::HT_MODIFIER_CTRL;

        if (hasShift && keysym >= XKB_KEY_a && keysym <= XKB_KEY_z)
            keysym = keysym - XKB_KEY_a + XKB_KEY_A;

        if (hasCtrl)
            oss << "^";

        oss << keysymToString(keysym);
    }
    return oss.str();
}

std::string CKeyAction::keysymToString(uint32_t keysym)
{
    static constexpr struct
    {
        uint32_t keysym;
        const char *name;
    } CUSTOM[] = {
        {XKB_KEY_A, "A"}, {XKB_KEY_B, "B"}, {XKB_KEY_C, "C"}, {XKB_KEY_D, "D"},
        {XKB_KEY_E, "E"}, {XKB_KEY_F, "F"}, {XKB_KEY_G, "G"}, {XKB_KEY_H, "H"},
        {XKB_KEY_I, "I"}, {XKB_KEY_J, "J"}, {XKB_KEY_K, "K"}, {XKB_KEY_L, "L"},
        {XKB_KEY_M, "M"}, {XKB_KEY_N, "N"}, {XKB_KEY_O, "O"}, {XKB_KEY_P, "P"},
        {XKB_KEY_Q, "Q"}, {XKB_KEY_R, "R"}, {XKB_KEY_S, "S"}, {XKB_KEY_T, "T"},
        {XKB_KEY_U, "U"}, {XKB_KEY_V, "V"}, {XKB_KEY_W, "W"}, {XKB_KEY_X, "X"},
        {XKB_KEY_Y, "Y"}, {XKB_KEY_Z, "Z"},
        {XKB_KEY_a, "a"}, {XKB_KEY_b, "b"}, {XKB_KEY_c, "c"}, {XKB_KEY_d, "d"},
        {XKB_KEY_e, "e"}, {XKB_KEY_f, "f"}, {XKB_KEY_g, "g"}, {XKB_KEY_h, "h"},
        {XKB_KEY_i, "i"}, {XKB_KEY_j, "j"}, {XKB_KEY_k, "k"}, {XKB_KEY_l, "l"},
        {XKB_KEY_m, "m"}, {XKB_KEY_n, "n"}, {XKB_KEY_o, "o"}, {XKB_KEY_p, "p"},
        {XKB_KEY_q, "q"}, {XKB_KEY_r, "r"}, {XKB_KEY_s, "s"}, {XKB_KEY_t, "t"},
        {XKB_KEY_u, "u"}, {XKB_KEY_v, "v"}, {XKB_KEY_w, "w"}, {XKB_KEY_x, "x"},
        {XKB_KEY_y, "y"}, {XKB_KEY_z, "z"},
        {XKB_KEY_Up, "↑"},
        {XKB_KEY_Down, "↓"},
        {XKB_KEY_Left, "←"},
        {XKB_KEY_Right, "→"},
        {XKB_KEY_Return, "Enter"},
        {XKB_KEY_Escape, "Esc"},
        {XKB_KEY_BackSpace, "Backspace"},
        {XKB_KEY_Tab, "Tab"},
        {XKB_KEY_space, "Space"},
        {XKB_KEY_Delete, "Del"},
        {XKB_KEY_Home, "Home"},
        {XKB_KEY_End, "End"},
        {XKB_KEY_Page_Up, "PgUp"},
        {XKB_KEY_Page_Down, "PgDn"},
        {XKB_KEY_slash, "/"},
        {XKB_KEY_period, "."},
    };

    for (const auto& [ks, name] : CUSTOM)
    {
        if (keysym == ks)
            return name;
    }

    if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F12) {
        return "F" + std::to_string(keysym - XKB_KEY_F1 + 1);
    }

    char name[64];
    if (xkb_keysym_get_name(keysym, name, sizeof(name)) == 0)
    {
        if (name[0] == 'a' && name[1] == 'c' && name[2] == '_')
            return std::string(name + 3);
        return name;
    }

    return "?";
}

CKeyBindings::CKeyBindings()
{
    moveDown.setDescription("Move down")
        .setRepeatable()
        .addBinding(XKB_KEY_j)
        .addBinding(XKB_KEY_Down);

    moveUp.setDescription("Move up")
        .setRepeatable()
        .addBinding(XKB_KEY_k)
        .addBinding(XKB_KEY_Up);

    goToParent.setDescription("Go to parent or previous preview")
        .addBinding(XKB_KEY_h)
        .addBinding(XKB_KEY_Left)
        .addBinding(XKB_KEY_BackSpace);

    goToChild.setDescription("Go to child or next preview")
        .addBinding(XKB_KEY_l)
        .addBinding(XKB_KEY_Right);

    openSelection.setDescription("Open selected item")
        .addBinding(XKB_KEY_Return);

    goToTop.setDescription("Go to top")
        .addBinding(XKB_KEY_g);

    goToBottom.setDescription("Go to bottom")
        .addBinding(XKB_KEY_G);

    pageDown.setDescription("Page down")
        .setRepeatable()
        .addBinding(XKB_KEY_Page_Down)
        .addBinding(XKB_KEY_j, HT_MODIFIER_CTRL);

    pageUp.setDescription("Page up")
        .setRepeatable()
        .addBinding(XKB_KEY_Page_Up)
        .addBinding(XKB_KEY_k, HT_MODIFIER_CTRL);

    toggleHidden.setDescription("Toggle hidden files")
        .addBinding(XKB_KEY_period);

    refresh.setDescription("Refresh")
        .addBinding(XKB_KEY_r);

    trash.setDescription("Move selected items to trash")
        .addBinding(XKB_KEY_Delete)
        .addBinding(XKB_KEY_D);

    search.setDescription("Search")
        .addBinding(XKB_KEY_slash);

    nextSearchResult.setDescription("Jump to next search result")
        .setRepeatable()
        .addBinding(XKB_KEY_n);

    previousSearchResult.setDescription("Jump to previous search result")
        .setRepeatable()
        .addBinding(XKB_KEY_N);

    startSelection.setDescription("Start range selection")
        .addBinding(XKB_KEY_v);

    openTerminal.setDescription("Open terminal in current folder")
        .addBinding(XKB_KEY_t);

    openNewWindow.setDescription("Open new window in current folder")
        .addBinding(XKB_KEY_T);

    copySelection.setDescription("Copy selected items")
        .addBinding(XKB_KEY_y);

    cutSelection.setDescription("Cut selected items")
        .addBinding(XKB_KEY_Y);

    pasteSelection.setDescription("Paste items")
        .addBinding(XKB_KEY_p);

    quit.setDescription("Quit or exit fullscreen")
        .addBinding(XKB_KEY_q)
        .addBinding(XKB_KEY_Escape);

    showHelp.setDescription("Show help")
        .addBinding(XKB_KEY_H);

    buildKeyBindingsMap();
}

void CKeyBindings::buildKeyBindingsMap()
{
    auto addBindings = [this](CKeyAction &action)
    {
        for (const auto &binding : action.getBindings())
        {
            bindingsMap_[SKeyCombo(binding.xkbKeysym, normalizeModifierMask(binding.modMask))] = &action;
        }
    };

    addBindings(moveDown);
    addBindings(moveUp);
    addBindings(goToParent);
    addBindings(goToChild);
    addBindings(openSelection);
    addBindings(goToTop);
    addBindings(goToBottom);
    addBindings(pageDown);
    addBindings(pageUp);
    addBindings(toggleHidden);
    addBindings(refresh);
    addBindings(trash);
    addBindings(search);
    addBindings(nextSearchResult);
    addBindings(previousSearchResult);
    addBindings(startSelection);
    addBindings(openTerminal);
    addBindings(openNewWindow);
    addBindings(copySelection);
    addBindings(cutSelection);
    addBindings(pasteSelection);
    addBindings(quit);
    addBindings(showHelp);
}

std::vector<std::pair<std::string, std::string>> CKeyBindings::getHelp() const
{
    return {
        {moveDown.getKeyText(), moveDown.getDescription()},
        {moveUp.getKeyText(), moveUp.getDescription()},
        {goToParent.getKeyText(), goToParent.getDescription()},
        {goToChild.getKeyText(), goToChild.getDescription()},
        {openSelection.getKeyText(), openSelection.getDescription()},
        {goToTop.getKeyText(), goToTop.getDescription()},
        {goToBottom.getKeyText(), goToBottom.getDescription()},
        {pageDown.getKeyText(), pageDown.getDescription()},
        {pageUp.getKeyText(), pageUp.getDescription()},

        {toggleHidden.getKeyText(), toggleHidden.getDescription()},
        {refresh.getKeyText(), refresh.getDescription()},
        {trash.getKeyText(), trash.getDescription()},
        {search.getKeyText(), search.getDescription()},
        {nextSearchResult.getKeyText(), nextSearchResult.getDescription()},
        {previousSearchResult.getKeyText(), previousSearchResult.getDescription()},
        {startSelection.getKeyText(), startSelection.getDescription()},
        {openTerminal.getKeyText(), openTerminal.getDescription()},
        {openNewWindow.getKeyText(), openNewWindow.getDescription()},
        {copySelection.getKeyText(), copySelection.getDescription()},
        {cutSelection.getKeyText(), cutSelection.getDescription()},
        {pasteSelection.getKeyText(), pasteSelection.getDescription()},
        {quit.getKeyText(), quit.getDescription()},
        {showHelp.getKeyText(), showHelp.getDescription()},
    };
}
