#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

/// Extracts a single thumbnail frame from a video file using FFmpeg libraries.
/// Completely synchronous — no event loop, no threading concerns.
class VideoThumbnail
{
public:
    struct Frame
    {
        std::vector<uint8_t> pixels; // RGB24 pixel data, tightly packed
        int width = 0;
        int height = 0;
    };

    struct EncodedFrame
    {
        std::vector<uint8_t> pngData;
        int width = 0;
        int height = 0;
    };

    /// Extract a thumbnail from the given video file.
    /// Seeks to ~1 second (or first keyframe) and decodes one frame,
    /// then scales it to fit within maxWidth x maxHeight while preserving
    /// the aspect ratio.
    /// Returns an empty Frame on failure.
    static Frame extract(const std::filesystem::path& path, int maxWidth, int maxHeight);

    /// Extract a thumbnail and return it as encoded PNG bytes.
    /// Returns an empty EncodedFrame on failure.
    static EncodedFrame extractToPngData(const std::filesystem::path& videoPath,
                                         int maxWidth, int maxHeight);
};
