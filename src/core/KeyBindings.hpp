#pragma once

#include <xkbcommon/xkbcommon.h>
#include <hyprtoolkit/core/Input.hpp>
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>

struct SKeyBinding
{
    uint32_t xkbKeysym = 0;
    uint32_t modMask = 0;

    SKeyBinding(uint32_t keysym, uint32_t mods = 0)
        : xkbKeysym(keysym), modMask(mods) {}
};

class CKeyAction
{
public:
    CKeyAction() = default;

    CKeyAction& setDescription(std::string description);
    CKeyAction& addBinding(uint32_t keysym, uint32_t mods = 0);
    CKeyAction& setRepeatable(bool repeatable = true);

    std::string getKeyText() const;
    const std::string& getDescription() const { return description_; }
    const std::vector<SKeyBinding>& getBindings() const { return bindings_; }
    void operator()() const { if (action_) action_(); }
    void operator()(int count) const
    {
        if (countAction_)
            countAction_(count);
        else if (action_)
            action_();
    }
    bool hasAction() const { return action_ != nullptr || countAction_ != nullptr; }
    void setAction(std::function<void()> action) { action_ = std::move(action); }
    void setCountAction(std::function<void(int)> action) { countAction_ = std::move(action); }
    bool hasCountAction() const { return countAction_ != nullptr; }
    bool isRepeatable() const { return repeatable_; }

    static std::string keysymToString(uint32_t keysym);

private:
    std::vector<SKeyBinding> bindings_;
    std::string description_;
    std::function<void()> action_;
    std::function<void(int)> countAction_;
    bool repeatable_ = false;
};

struct SKeyCombo
{
    uint32_t keysym = 0;
    uint32_t modMask = 0;

    SKeyCombo(uint32_t keysym = 0, uint32_t modMask = 0)
        : keysym(keysym), modMask(modMask) {}

    bool operator==(const SKeyCombo &other) const
    {
        return keysym == other.keysym && modMask == other.modMask;
    }
};

inline uint32_t normalizeModifierMask(uint32_t modMask)
{
    constexpr uint32_t relevant = Hyprtoolkit::Input::HT_MODIFIER_SHIFT | Hyprtoolkit::Input::HT_MODIFIER_CTRL | Hyprtoolkit::Input::HT_MODIFIER_ALT | Hyprtoolkit::Input::HT_MODIFIER_META;
    return modMask & relevant;
}

struct SKeyComboHash
{
    size_t operator()(SKeyCombo k) const
    {
        return std::hash<uint64_t>{}((uint64_t(k.keysym) << 32) | k.modMask);
    }
};

class CKeyBindings
{
public:
    CKeyBindings();

    std::vector<std::pair<std::string, std::string>> getHelp() const;

    const std::unordered_map<SKeyCombo, CKeyAction*, SKeyComboHash> &getBindingsMap() const { return bindingsMap_; }

    CKeyAction moveDown;
    CKeyAction moveUp;
    CKeyAction goToParent;
    CKeyAction goToChild;
    CKeyAction openSelection;
    CKeyAction goToTop;
    CKeyAction goToBottom;
    CKeyAction pageDown;
    CKeyAction pageUp;

    CKeyAction toggleHidden;
    CKeyAction refresh;
    CKeyAction trash;
    CKeyAction search;
    CKeyAction nextSearchResult;
    CKeyAction previousSearchResult;
    CKeyAction startSelection;
    CKeyAction openTerminal;
    CKeyAction openNewWindow;
    CKeyAction copySelection;
    CKeyAction cutSelection;
    CKeyAction pasteSelection;
    CKeyAction quit;
    CKeyAction showHelp;

private:
    std::unordered_map<SKeyCombo, CKeyAction*, SKeyComboHash> bindingsMap_;

    void buildKeyBindingsMap();
};
