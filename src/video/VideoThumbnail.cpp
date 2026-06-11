#include "VideoThumbnail.hpp"
#include "VideoThumbnailSeek.hpp"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <algorithm>

VideoThumbnail::Frame VideoThumbnail::extract(const std::filesystem::path& path, int maxWidth, int maxHeight)
{
    Frame result;

    if (maxWidth <= 0 || maxHeight <= 0)
        return result;

    // Open the file.
    AVFormatContext* fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, path.c_str(), nullptr, nullptr) < 0)
        return result;

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
    {
        avformat_close_input(&fmtCtx);
        return result;
    }

    // Find the best video stream.
    const int streamIdx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIdx < 0)
    {
        avformat_close_input(&fmtCtx);
        return result;
    }

    const AVStream* stream = fmtCtx->streams[streamIdx];
    const AVCodecParameters* codecPar = stream->codecpar;

    // Find decoder.
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec)
    {
        avformat_close_input(&fmtCtx);
        return result;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx)
    {
        avformat_close_input(&fmtCtx);
        return result;
    }

    if (avcodec_parameters_to_context(codecCtx, codecPar) < 0)
    {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return result;
    }

    if (avcodec_open2(codecCtx, codec, nullptr) < 0)
    {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return result;
    }

    // Seek to ~1 second to skip black intro frames.
    // Use the stream's time_base for accurate seeking.
    if (stream->duration > 0)
    {
        const int64_t seekTarget = VideoThumbnailDetail::calculateSeekTarget(
            stream->duration,
            stream->time_base.num,
            stream->time_base.den);
        if (seekTarget > 0)
            av_seek_frame(fmtCtx, streamIdx, seekTarget, AVSEEK_FLAG_BACKWARD);
    }

    // Decode one frame.
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!pkt || !frame)
    {
        av_packet_free(&pkt);
        av_frame_free(&frame);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return result;
    }

    bool gotFrame = false;
    // Read packets until we decode a video frame (skip non-video packets).
    while (av_read_frame(fmtCtx, pkt) >= 0)
    {
        if (pkt->stream_index != streamIdx)
        {
            av_packet_unref(pkt);
            continue;
        }

        if (avcodec_send_packet(codecCtx, pkt) >= 0)
        {
            if (avcodec_receive_frame(codecCtx, frame) >= 0)
            {
                gotFrame = true;
                av_packet_unref(pkt);
                break;
            }
        }
        av_packet_unref(pkt);
    }

    if (!gotFrame)
    {
        // Flush the decoder in case the frame is buffered.
        avcodec_send_packet(codecCtx, nullptr);
        if (avcodec_receive_frame(codecCtx, frame) >= 0)
            gotFrame = true;
    }

    if (!gotFrame)
    {
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return result;
    }

    // Compute output size preserving aspect ratio.
    const int srcW = frame->width;
    const int srcH = frame->height;
    const double scale = std::min(
        static_cast<double>(maxWidth) / srcW,
        static_cast<double>(maxHeight) / srcH);
    const int dstW = std::max(1, static_cast<int>(srcW * scale));
    const int dstH = std::max(1, static_cast<int>(srcH * scale));

    // Convert to RGB24.
    SwsContext* swsCtx = sws_getContext(
        srcW, srcH, static_cast<AVPixelFormat>(frame->format),
        dstW, dstH, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!swsCtx)
    {
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return result;
    }

    const int dstStride = dstW * 3;
    result.pixels.resize(static_cast<size_t>(dstStride) * dstH);
    uint8_t* dstData[1] = {result.pixels.data()};
    int dstLinesize[1] = {dstStride};

    sws_scale(swsCtx, frame->data, frame->linesize, 0, srcH, dstData, dstLinesize);

    result.width = dstW;
    result.height = dstH;

    sws_freeContext(swsCtx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    return result;
}

VideoThumbnail::EncodedFrame VideoThumbnail::extractToPngData(const std::filesystem::path& videoPath,
                                                              int maxWidth, int maxHeight)
{
    EncodedFrame result;

    auto frame = extract(videoPath, maxWidth, maxHeight);
    if (frame.pixels.empty() || frame.width <= 0 || frame.height <= 0)
        return result;

    const int w = frame.width;
    const int h = frame.height;
    const int rowStride = w * 3;

    // Create a GdkPixbuf from the RGB24 data and encode it as PNG bytes.
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_data(
        frame.pixels.data(),
        GDK_COLORSPACE_RGB,
        FALSE,           // no alpha
        8,               // bits per sample
        w, h,
        rowStride,
        nullptr,         // destroy_fn (we manage the data)
        nullptr);

    if (!pixbuf)
        return result;

    GError* err = nullptr;
    gchar* buffer = nullptr;
    gsize bufferSize = 0;
    gboolean ok = gdk_pixbuf_save_to_buffer(pixbuf, &buffer, &bufferSize, "png", &err, nullptr);
    g_object_unref(pixbuf);

    if (err)
    {
        g_error_free(err);
        if (buffer)
            g_free(buffer);
        return result;
    }

    if (ok != TRUE || !buffer || bufferSize == 0)
    {
        if (buffer)
            g_free(buffer);
        return result;
    }

    const auto* pngBegin = reinterpret_cast<const uint8_t*>(buffer);
    result.pngData.assign(pngBegin, pngBegin + bufferSize);
    result.width = w;
    result.height = h;

    g_free(buffer);
    return result;
}
