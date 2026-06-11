#include <gtest/gtest.h>

#include <filesystem>

#include "ui/TopBar.hpp"

TEST(TopBarTests, DisplayPathUsesDirectoryOutsideFullscreen)
{
    const std::filesystem::path directory = "/home/test/Pictures";
    const std::filesystem::path selectedItem = directory / "image.png";

    EXPECT_EQ(topBarDisplayPath(directory, selectedItem, false), directory);
}

TEST(TopBarTests, DisplayPathUsesSelectedItemInsideFullscreen)
{
    const std::filesystem::path directory = "/home/test/Pictures";
    const std::filesystem::path selectedItem = directory / "image.png";

    EXPECT_EQ(topBarDisplayPath(directory, selectedItem, true), selectedItem);
}
