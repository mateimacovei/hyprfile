#pragma once

#include <cstdint>

namespace VideoThumbnailDetail
{
    inline int64_t calculateSeekTarget(int64_t duration, int timeBaseNum, int timeBaseDen)
    {
        if (duration <= 0 || timeBaseNum <= 0 || timeBaseDen <= 0)
            return 0;

        int64_t seekTarget = timeBaseDen / timeBaseNum;
        if (seekTarget > duration / 2)
            seekTarget = duration / 4;

        return seekTarget;
    }
}
