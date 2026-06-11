#include <gtest/gtest.h>

#include "../src/video/VideoThumbnailSeek.hpp"

TEST(VideoThumbnailSeekTests, InvalidTimeBaseNumeratorDoesNotSeek)
{
    EXPECT_EQ(VideoThumbnailDetail::calculateSeekTarget(120, 0, 1000), 0);
}

TEST(VideoThumbnailSeekTests, InvalidTimeBaseDenominatorDoesNotSeek)
{
    EXPECT_EQ(VideoThumbnailDetail::calculateSeekTarget(120, 1, 0), 0);
}

TEST(VideoThumbnailSeekTests, NegativeTimeBaseNumeratorDoesNotSeek)
{
    EXPECT_EQ(VideoThumbnailDetail::calculateSeekTarget(120, -1, 1000), 0);
}

TEST(VideoThumbnailSeekTests, NegativeTimeBaseDenominatorDoesNotSeek)
{
    EXPECT_EQ(VideoThumbnailDetail::calculateSeekTarget(120, 1, -1000), 0);
}

TEST(VideoThumbnailSeekTests, CalculatesOneSecondInStreamTimeBase)
{
    EXPECT_EQ(VideoThumbnailDetail::calculateSeekTarget(3000, 1, 1000), 1000);
}

TEST(VideoThumbnailSeekTests, DoesNotSeekPastHalfDuration)
{
    EXPECT_EQ(VideoThumbnailDetail::calculateSeekTarget(1200, 1, 1000), 300);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
