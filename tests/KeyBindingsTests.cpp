#include <gtest/gtest.h>
#include <xkbcommon/xkbcommon.h>

#include <algorithm>

#include "../src/core/KeyBindings.hpp"

TEST(CKeyBindingsTests, SetsDescriptionsForAllActions)
{
    CKeyBindings bindings;

    auto help = bindings.getHelp();

    for (const auto &[keys, desc] : help)
    {
        EXPECT_FALSE(desc.empty()) << "Action with keys '" << keys << "' has empty description";
    }
}

TEST(CKeyBindingsTests, NavigationActionsHaveBindings)
{
    CKeyBindings bindings;

    EXPECT_FALSE(bindings.moveDown.getKeyText().empty());
    EXPECT_FALSE(bindings.moveUp.getKeyText().empty());
    EXPECT_FALSE(bindings.goToParent.getKeyText().empty());
    EXPECT_FALSE(bindings.goToChild.getKeyText().empty());
    EXPECT_FALSE(bindings.openSelection.getKeyText().empty());
}

TEST(CKeyBindingsTests, MoveDownHasCorrectKeys)
{
    CKeyBindings bindings;

    EXPECT_EQ(bindings.moveDown.getDescription(), "Move down");
    EXPECT_EQ(bindings.moveDown.getKeyText(), "j, ↓");
}

TEST(CKeyBindingsTests, MoveUpHasCorrectKeys)
{
    CKeyBindings bindings;

    EXPECT_EQ(bindings.moveUp.getDescription(), "Move up");
    EXPECT_EQ(bindings.moveUp.getKeyText(), "k, ↑");
}

TEST(CKeyBindingsTests, OpenSelectionHasCorrectKeys)
{
    CKeyBindings bindings;

    EXPECT_EQ(bindings.openSelection.getDescription(), "Open selected item");
    EXPECT_EQ(bindings.openSelection.getKeyText(), "Enter");
}

TEST(CKeyBindingsTests, QuitHasCorrectKeys)
{
    CKeyBindings bindings;

    EXPECT_EQ(bindings.quit.getDescription(), "Quit or exit fullscreen");
    EXPECT_EQ(bindings.quit.getKeyText(), "q, Esc");
}

TEST(CKeyBindingsTests, RemoveHasCorrectKeys)
{
    CKeyBindings bindings;

    EXPECT_EQ(bindings.trash.getDescription(), "Move selected items to trash");
    EXPECT_EQ(bindings.trash.getKeyText(), "Del, D");
}

TEST(CKeyBindingsTests, PageUpHasCorrectKeys)
{
    CKeyBindings bindings;

    EXPECT_EQ(bindings.pageUp.getDescription(), "Page up");
    EXPECT_EQ(bindings.pageUp.getKeyText(), "PgUp, ^k");
}

TEST(CKeyBindingsTests, PendingFileOperationKeysMatchVimStyleBindings)
{
    CKeyBindings bindings;
    const auto &map = bindings.getBindingsMap();

    EXPECT_EQ(bindings.nextSearchResult.getDescription(), "Jump to next search result");
    EXPECT_EQ(bindings.nextSearchResult.getKeyText(), "n");
    ASSERT_NE(map.find({XKB_KEY_n, 0}), map.end());
    EXPECT_EQ(map.at({XKB_KEY_n, 0}), &bindings.nextSearchResult);

    EXPECT_EQ(bindings.previousSearchResult.getDescription(), "Jump to previous search result");
    EXPECT_EQ(bindings.previousSearchResult.getKeyText(), "N");
    ASSERT_NE(map.find({XKB_KEY_N, 0}), map.end());
    EXPECT_EQ(map.at({XKB_KEY_N, 0}), &bindings.previousSearchResult);

    EXPECT_EQ(bindings.startSelection.getDescription(), "Start range selection");
    EXPECT_EQ(bindings.startSelection.getKeyText(), "v");
    ASSERT_NE(map.find({XKB_KEY_v, 0}), map.end());
    EXPECT_EQ(map.at({XKB_KEY_v, 0}), &bindings.startSelection);

    EXPECT_EQ(bindings.openTerminal.getDescription(), "Open terminal in current folder");
    EXPECT_EQ(bindings.openTerminal.getKeyText(), "t");
    ASSERT_NE(map.find({XKB_KEY_t, 0}), map.end());
    EXPECT_EQ(map.at({XKB_KEY_t, 0}), &bindings.openTerminal);

    EXPECT_EQ(bindings.copySelection.getDescription(), "Copy selected items");
    EXPECT_EQ(bindings.copySelection.getKeyText(), "y");
    ASSERT_NE(map.find({XKB_KEY_y, 0}), map.end());
    EXPECT_EQ(map.at({XKB_KEY_y, 0}), &bindings.copySelection);

    EXPECT_EQ(bindings.cutSelection.getDescription(), "Cut selected items");
    EXPECT_EQ(bindings.cutSelection.getKeyText(), "Y");
    ASSERT_NE(map.find({XKB_KEY_Y, 0}), map.end());
    EXPECT_EQ(map.at({XKB_KEY_Y, 0}), &bindings.cutSelection);

    EXPECT_EQ(bindings.pasteSelection.getDescription(), "Paste items");
    EXPECT_EQ(bindings.pasteSelection.getKeyText(), "p");
    ASSERT_NE(map.find({XKB_KEY_p, 0}), map.end());
    EXPECT_EQ(map.at({XKB_KEY_p, 0}), &bindings.pasteSelection);

    EXPECT_EQ(map.find({XKB_KEY_space, 0}), map.end());
}

TEST(CKeyBindingsTests, OpenNewWindowIsBoundToShiftT)
{
    CKeyBindings bindings;
    const auto &map = bindings.getBindingsMap();

    ASSERT_NE(map.find({XKB_KEY_T, 0}), map.end());

    const auto help = bindings.getHelp();
    const auto helpEntry = std::find(help.begin(), help.end(),
                                     std::pair<std::string, std::string>{"T", "Open new window in current folder"});
    EXPECT_NE(helpEntry, help.end());
}

TEST(CKeyBindingsTests, SearchResultNavigationActionsAreRepeatable)
{
    CKeyBindings bindings;

    EXPECT_TRUE(bindings.moveDown.isRepeatable());
    EXPECT_TRUE(bindings.moveUp.isRepeatable());
    EXPECT_TRUE(bindings.pageDown.isRepeatable());
    EXPECT_TRUE(bindings.pageUp.isRepeatable());
    EXPECT_TRUE(bindings.nextSearchResult.isRepeatable());
    EXPECT_TRUE(bindings.previousSearchResult.isRepeatable());
    EXPECT_FALSE(bindings.search.isRepeatable());
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
