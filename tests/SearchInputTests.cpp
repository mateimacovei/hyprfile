#include <gtest/gtest.h>
#include <hyprtoolkit/core/Input.hpp>
#include <xkbcommon/xkbcommon.h>

#include "../src/core/SearchInput.hpp"

TEST(SearchInputTests, AppendsShiftedPrintableCharacters)
{
    std::string buffer;

    const bool appended = SearchInput::appendKeysym(buffer, XKB_KEY_A, Hyprtoolkit::Input::HT_MODIFIER_SHIFT);

    EXPECT_TRUE(appended);
    EXPECT_EQ(buffer, "A");
}

TEST(SearchInputTests, AppendsMultiplePrintableCharactersWithoutEmbeddedNull)
{
    std::string buffer;

    EXPECT_TRUE(SearchInput::appendKeysym(buffer, XKB_KEY_a, 0));
    EXPECT_TRUE(SearchInput::appendKeysym(buffer, XKB_KEY_b, 0));

    EXPECT_EQ(buffer, "ab");
    EXPECT_EQ(buffer.size(), 2U);
}

TEST(SearchInputTests, IgnoresControlModifiedPrintableCharacters)
{
    std::string buffer;

    const bool appended = SearchInput::appendKeysym(buffer, XKB_KEY_a, Hyprtoolkit::Input::HT_MODIFIER_CTRL);

    EXPECT_FALSE(appended);
    EXPECT_TRUE(buffer.empty());
}

TEST(SearchInputTests, RemovesLastUtf8Codepoint)
{
    std::string buffer = "a\xC3\xA9";

    SearchInput::popLastUtf8Codepoint(buffer);

    EXPECT_EQ(buffer, "a");
}

TEST(SearchInputTests, TreatsEscapeAndLowercaseQAsCancelKeys)
{
    EXPECT_TRUE(SearchInput::isCancelKeysym(XKB_KEY_Escape));
    EXPECT_TRUE(SearchInput::isCancelKeysym(XKB_KEY_q));
    EXPECT_FALSE(SearchInput::isCancelKeysym(XKB_KEY_Q));
    EXPECT_FALSE(SearchInput::isCancelKeysym(XKB_KEY_a));
}
