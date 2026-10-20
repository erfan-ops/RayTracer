#include <Image.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace {

// ============================================================
// RAII wrappers for FFmpeg resources
// ============================================================

// The free functions differ in signature: some take a pointer to the pointer
// so they can clear the caller's variable, others take the pointer directly.
// Each deleter normalises that difference so std::unique_ptr can own the
// resource and release it on every path, including stack unwinding.

struct CodecContextDeleter {
    void operator()(AVCodecContext* context) const noexcept {
        if (context != nullptr) {
            avcodec_free_context(&context);
        }
    }
};

struct FrameDeleter {
    void operator()(AVFrame* frame) const noexcept {
        if (frame != nullptr) {
            av_frame_free(&frame);
        }
    }
};

struct PacketDeleter {
    void operator()(AVPacket* packet) const noexcept {
        if (packet != nullptr) {
            av_packet_free(&packet);
        }
    }
};

// libavformat treats the output AVIOContext as belonging to the format
// context, so it is released here rather than by a second owner. Closing the
// I/O context before freeing the format context is also the order libavformat
// itself documents (and the one its own examples use); avio_closep() clears
// the field, so no dangling pointer is left behind. For muxers flagged
// AVFMT_NOFILE the field is null and this is a no-op.
struct FormatContextDeleter {
    void operator()(AVFormatContext* context) const noexcept {
        if (context != nullptr) {
            avio_closep(&context->pb);
            avformat_free_context(context);
        }
    }
};

struct SwsContextDeleter {
    void operator()(SwsContext* context) const noexcept {
        if (context != nullptr) {
            sws_freeContext(context);
        }
    }
};

using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

// ============================================================
// Diagnostics
// ============================================================

/// Sentinel meaning "leave this encoder option at its default".
constexpr int kEncoderDefault = -1;

/// Renders an FFmpeg error code as human-readable text.
[[nodiscard]] std::string ffmpegErrorText(const int errorCode) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    if (av_strerror(errorCode, buffer, sizeof(buffer)) < 0) {
        return "unrecognised error " + std::to_string(errorCode);
    }
    return std::string(buffer);
}

/// Builds the message for an operation that failed, naming the operation and
/// the file it was working on.
[[nodiscard]] std::string failureMessage(const char* operation, const int errorCode,
                                        const std::string& pathText) {
    std::ostringstream message;
    message << operation << " for '" << pathText << "' failed: " << ffmpegErrorText(errorCode)
            << " (FFmpeg error " << errorCode << ')';
    return message.str();
}

[[noreturn]] void throwFfmpegError(const char* operation, const int errorCode,
                                   const std::string& pathText) {
    throw std::runtime_error(failureMessage(operation, errorCode, pathText));
}

/// FFmpeg's file protocol expects UTF-8 on Windows, which is not what
/// std::filesystem::path::string() produces for non-ASCII names.
[[nodiscard]] std::string toUtf8String(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

// ============================================================
// Format mapping
// ============================================================

[[nodiscard]] constexpr int bytesPerPixelFor(const ImageWrite::PixelFormat format) noexcept {
    switch (format) {
        case ImageWrite::PixelFormat::RGB24: return 3;
        case ImageWrite::PixelFormat::RGBA32: return 4;
        case ImageWrite::PixelFormat::GRAY8: return 1;
    }
    return 0;
}

/// Pixel format of the caller's buffer, which always lives in one plane
/// because every supported input layout is interleaved.
[[nodiscard]] constexpr AVPixelFormat sourcePixelFormatFor(const ImageWrite::PixelFormat format) noexcept {
    switch (format) {
        case ImageWrite::PixelFormat::RGB24: return AV_PIX_FMT_RGB24;
        case ImageWrite::PixelFormat::RGBA32: return AV_PIX_FMT_RGBA;
        case ImageWrite::PixelFormat::GRAY8: return AV_PIX_FMT_GRAY8;
    }
    return AV_PIX_FMT_NONE;
}

[[nodiscard]] const char* codecNameFor(const ImageWrite::Format format) noexcept {
    switch (format) {
        // WebP has no native encoder in FFmpeg; it is provided by libwebp,
        // which this build was compiled with.
        case ImageWrite::Format::WebP: return "libwebp";
        case ImageWrite::Format::PNG: return "png";
        case ImageWrite::Format::JPEG: return "mjpeg";
        case ImageWrite::Format::BMP: return "bmp";
    }
    return nullptr;
}

/// Muxer used to write the file. It is named explicitly so that the output
/// format is never inferred from the file extension.
[[nodiscard]] constexpr const char* muxerNameFor(const ImageWrite::Format format) noexcept {
    switch (format) {
        case ImageWrite::Format::PNG:
        case ImageWrite::Format::JPEG:
        case ImageWrite::Format::BMP:
            return "image2";
        case ImageWrite::Format::WebP:
            return "webp";
    }
    return nullptr;
}

/// Pixel format the encoder is fed, which is rarely the format the caller
/// supplied: none of these encoders accept RGB24 except PNG.
[[nodiscard]] constexpr AVPixelFormat targetPixelFormatFor(const ImageWrite::Format format,
                                                           const ImageWrite::PixelFormat source) noexcept {
    switch (format) {
        case ImageWrite::Format::PNG:
            // PNG stores the caller's channels as-is.
            switch (source) {
                case ImageWrite::PixelFormat::RGB24: return AV_PIX_FMT_RGB24;
                case ImageWrite::PixelFormat::RGBA32: return AV_PIX_FMT_RGBA;
                case ImageWrite::PixelFormat::GRAY8: return AV_PIX_FMT_GRAY8;
            }
            break;
        case ImageWrite::Format::JPEG:
            // The MJPEG encoder accepts only planar YCbCr; 4:2:0 keeps files
            // small and matches the usual JPEG convention.
            return AV_PIX_FMT_YUV420P;
        case ImageWrite::Format::BMP:
            // The BMP encoder has no RGB24 entry: it wants blue first.
            switch (source) {
                case ImageWrite::PixelFormat::RGB24: return AV_PIX_FMT_BGR24;
                case ImageWrite::PixelFormat::RGBA32: return AV_PIX_FMT_BGRA;
                case ImageWrite::PixelFormat::GRAY8: return AV_PIX_FMT_GRAY8;
            }
            break;
        case ImageWrite::Format::WebP:
            // libwebp converts BGRA to its internal YUV itself, which is
            // measurably more accurate than converting to YUV here and
            // guessing the range convention it expects. Lossless WebP
            // requires BGRA anyway.
            return AV_PIX_FMT_BGRA;
    }
    return AV_PIX_FMT_NONE;
}

/// Whether the encoder expects full-range (JPEG-style) chroma. libavcodec 63
/// deprecates the yuvj* pixel formats in favour of a plain yuv format plus an
/// explicit range, so the range is carried separately and applied to the
/// scaler as well.
[[nodiscard]] constexpr bool needsFullRangeOutput(const ImageWrite::Format format) noexcept {
    return format == ImageWrite::Format::JPEG;
}

/// Maps a conventional "higher is better" JPEG quality (1..100) onto the
/// quantiser scale the MJPEG encoder actually honours (1..31, lower is
/// better). Verified against libavcodec 63: setting global_quality alongside
/// AV_CODEC_FLAG_QSCALE has no effect on this encoder, whereas qmin/qmax does.
[[nodiscard]] constexpr int jpegQuantiserFor(const int quality) noexcept {
    constexpr int kBestQuantiser = 1;
    constexpr int kWorstQuantiser = 31;
    constexpr int kSpan = kWorstQuantiser - kBestQuantiser;
    const int quantiser = kWorstQuantiser - (quality * kSpan + 50) / 100;
    return std::clamp(quantiser, kBestQuantiser, kWorstQuantiser);
}

// ============================================================
// Failure clean-up
// ============================================================

/// Removes the output file unless the save operation completed, so a failure
/// cannot leave a truncated image behind for the caller to mistake for a
/// successful result.
///
/// Two-phase on purpose: the guard has to be declared before the format
/// context so that it is destroyed *after* it, because libavformat's deleter
/// is what closes the file and Windows refuses to delete a file that is still
/// open. Arming is therefore a separate step, taken only once the operation
/// reaches the point where it can create or truncate the output -- everything
/// before that (argument validation, encoder set-up, pixel conversion) leaves
/// an existing file untouched.
class PartialFileGuard {
public:
    explicit PartialFileGuard(std::filesystem::path path) noexcept : path_(std::move(path)) {}

    PartialFileGuard(const PartialFileGuard&) = delete;
    PartialFileGuard& operator=(const PartialFileGuard&) = delete;
    PartialFileGuard(PartialFileGuard&&) = delete;
    PartialFileGuard& operator=(PartialFileGuard&&) = delete;

    ~PartialFileGuard() {
        if (!armed_) {
            return;
        }
        // Best effort: a destructor must not throw, and there is nothing
        // useful to do if the file cannot be removed.
        std::error_code ignored;
        // Only a regular file is ever removed; the path may name a directory
        // the caller passed by mistake, and remove() would delete an empty one.
        if (std::filesystem::is_regular_file(path_, ignored)) {
            std::filesystem::remove(path_, ignored);
        }
    }

    /// Starts watching for a partially written output file.
    void arm() noexcept { armed_ = true; }

    /// Marks the operation as complete, keeping the output file.
    void commit() noexcept { armed_ = false; }

private:
    std::filesystem::path path_;
    bool armed_ = false;
};

// ============================================================
// Shared encoding pipeline
// ============================================================

/// Everything that varies between savePNG()/saveJPEG()/saveBMP()/saveWebP().
/// The FFmpeg specifics stay out of the public header by living here.
struct SaveRequest {
    ImageWrite::Format format;
    ImageWrite::PixelFormat sourceFormat;
    const unsigned char* data;
    int width;
    int height;
    int rowStrideBytes;
    int compressionLevel;
    int quality;
    bool lossless;
};

/// Encodes one image and writes it to @p outputPath. Shared by all four public
/// save methods; they differ only in the request they build.
void saveImage(const SaveRequest& request, const std::filesystem::path& outputPath) {
    const std::string pathText = toUtf8String(outputPath);

    // ---- Validate the caller's buffer ------------------------------------
    // Every read performed below is confined to the strides computed here, so
    // validating them up front is what keeps the encoder from running past the
    // end of the caller's allocation.
    if (request.data == nullptr) {
        throw std::invalid_argument("Cannot save '" + pathText + "': the pixel buffer is null");
    }
    if (request.width <= 0 || request.height <= 0) {
        std::ostringstream message;
        message << "Cannot save '" << pathText << "': image dimensions must be positive, got "
                << request.width << 'x' << request.height;
        throw std::invalid_argument(message.str());
    }

    const int bytesPerPixel = bytesPerPixelFor(request.sourceFormat);
    // Guard the multiplication below before performing it.
    if (request.width > std::numeric_limits<int>::max() / bytesPerPixel) {
        throw std::invalid_argument("Cannot save '" + pathText + "': image width is too large");
    }
    const int minimumStride = request.width * bytesPerPixel;

    int sourceStride = request.rowStrideBytes;
    if (sourceStride == 0) {
        sourceStride = minimumStride;
    } else if (sourceStride < minimumStride) {
        std::ostringstream message;
        message << "Cannot save '" << pathText << "': rowStrideBytes (" << request.rowStrideBytes
                << ") is smaller than one tightly packed row (" << minimumStride << " bytes)";
        throw std::invalid_argument(message.str());
    }

    // Let libavcodec reject dimensions it cannot represent, rather than
    // discovering the limit halfway through encoding.
    const int sizeCheck = av_image_check_size(static_cast<unsigned int>(request.width),
                                              static_cast<unsigned int>(request.height), 0, nullptr);
    if (sizeCheck < 0) {
        std::ostringstream message;
        message << "Cannot save '" << pathText << "': " << request.width << 'x' << request.height
                << " is not an image size libavcodec accepts: " << ffmpegErrorText(sizeCheck);
        throw std::invalid_argument(message.str());
    }

    if (request.format == ImageWrite::Format::PNG && request.compressionLevel != kEncoderDefault &&
        (request.compressionLevel < 0 || request.compressionLevel > 9)) {
        std::ostringstream message;
        message << "Cannot save '" << pathText << "': compressionLevel must be between 0 and 9, got "
                << request.compressionLevel;
        throw std::invalid_argument(message.str());
    }
    if ((request.format == ImageWrite::Format::JPEG || request.format == ImageWrite::Format::WebP) &&
        request.quality != kEncoderDefault && (request.quality < 1 || request.quality > 100)) {
        std::ostringstream message;
        message << "Cannot save '" << pathText << "': quality must be between 1 and 100, got "
                << request.quality;
        throw std::invalid_argument(message.str());
    }

    // ---- Encoder ---------------------------------------------------------
    const char* const codecName = codecNameFor(request.format);
    const AVCodec* const codec = codecName != nullptr ? avcodec_find_encoder_by_name(codecName) : nullptr;
    if (codec == nullptr) {
        throw std::runtime_error("Cannot save '" + pathText + "': this FFmpeg build has no '" +
                                 (codecName != nullptr ? codecName : "<unknown>") +
                                 "' encoder; use ImageWrite::isFormatAvailable() to test for one");
    }

    const AVPixelFormat sourceFormat = sourcePixelFormatFor(request.sourceFormat);
    const AVPixelFormat targetFormat = targetPixelFormatFor(request.format, request.sourceFormat);
    const bool fullRange = needsFullRangeOutput(request.format);

    CodecContextPtr codecContext(avcodec_alloc_context3(codec));
    if (!codecContext) {
        throw std::runtime_error("Cannot save '" + pathText + "': failed to allocate an encoder context");
    }
    codecContext->width = request.width;
    codecContext->height = request.height;
    codecContext->pix_fmt = targetFormat;
    codecContext->time_base = AVRational{1, 25};
    if (fullRange) {
        codecContext->color_range = AVCOL_RANGE_JPEG;
    }

    if (request.compressionLevel != kEncoderDefault) {
        codecContext->compression_level = request.compressionLevel;
    }
    if (request.format == ImageWrite::Format::JPEG && request.quality != kEncoderDefault) {
        const int quantiser = jpegQuantiserFor(request.quality);
        codecContext->qmin = quantiser;
        codecContext->qmax = quantiser;
    }
    if (request.format == ImageWrite::Format::WebP) {
        if (request.lossless) {
            const int ret = av_opt_set_int(codecContext->priv_data, "lossless", 1, 0);
            if (ret < 0) {
                throwFfmpegError("Enabling lossless WebP encoding", ret, pathText);
            }
        } else if (request.quality != kEncoderDefault) {
            const int ret = av_opt_set_int(codecContext->priv_data, "quality", request.quality, 0);
            if (ret < 0) {
                throwFfmpegError("Setting the WebP encoding quality", ret, pathText);
            }
        }
    }

    int result = avcodec_open2(codecContext.get(), codec, nullptr);
    if (result < 0) {
        throwFfmpegError("Opening the encoder", result, pathText);
    }

    // ---- Frame -----------------------------------------------------------
    FramePtr frame(av_frame_alloc());
    if (!frame) {
        throw std::runtime_error("Cannot save '" + pathText + "': failed to allocate a frame");
    }
    frame->format = targetFormat;
    frame->width = request.width;
    frame->height = request.height;
    result = av_frame_get_buffer(frame.get(), 0);
    if (result < 0) {
        throwFfmpegError("Allocating the frame buffer", result, pathText);
    }
    if (fullRange) {
        frame->color_range = AVCOL_RANGE_JPEG;
    }

    // ---- Pixel format conversion -----------------------------------------
    // All supported input layouts are interleaved single-plane buffers, so the
    // source is always described by one plane at the caller's stride.
    const std::uint8_t* sourcePlanes[4] = {request.data, nullptr, nullptr, nullptr};
    const int sourceStrides[4] = {sourceStride, 0, 0, 0};

    if (sourceFormat == targetFormat) {
        // The encoder already takes the caller's layout, so a stride-aware
        // copy is all that is needed.
        av_image_copy(frame->data, frame->linesize, sourcePlanes, sourceStrides, targetFormat,
                      request.width, request.height);
    } else {
        SwsContextPtr scaler(sws_getContext(request.width, request.height, sourceFormat,
                                            request.width, request.height, targetFormat, SWS_POINT,
                                            nullptr, nullptr, nullptr),
                             SwsContextDeleter{});
        if (!scaler) {
            throw std::runtime_error("Cannot save '" + pathText + "': failed to create a converter from " +
                                     av_get_pix_fmt_name(sourceFormat) + " to " +
                                     av_get_pix_fmt_name(targetFormat));
        }
        if (fullRange) {
            // Tell the scaler to emit full-range chroma, matching the range
            // advertised on the codec context and frame above.
            const int* const coefficients = sws_getCoefficients(SWS_CS_DEFAULT);
            constexpr int kFullRange = 1;
            result = sws_setColorspaceDetails(scaler.get(), coefficients, kFullRange, coefficients,
                                              kFullRange, 0, 1 << 16, 1 << 16);
            if (result < 0) {
                throwFfmpegError("Configuring the output colour range", result, pathText);
            }
        }
        const int convertedRows = sws_scale(scaler.get(), sourcePlanes, sourceStrides, 0, request.height,
                                            frame->data, frame->linesize);
        if (convertedRows < 0) {
            throwFfmpegError("Converting the pixel format", convertedRows, pathText);
        }
        if (convertedRows != request.height) {
            std::ostringstream message;
            message << "Converting the pixel format for '" << pathText << "' produced "
                    << convertedRows << " of " << request.height << " rows";
            throw std::runtime_error(message.str());
        }
    }
    frame->pts = 0;

    // ---- Muxer -----------------------------------------------------------
    // Declared ahead of the format context so that it outlives it, and thus
    // removes the output file only once the format context has closed it.
    PartialFileGuard outputGuard(outputPath);

    const char* const muxerName = muxerNameFor(request.format);
    AVFormatContext* rawContext = nullptr;
    result = avformat_alloc_output_context2(&rawContext, nullptr, muxerName, pathText.c_str());
    if (result < 0 || rawContext == nullptr) {
        // The function clears the out parameter before it can fail and frees
        // anything it allocated, so there is nothing to release here.
        throwFfmpegError("Creating the output context", result < 0 ? result : AVERROR(EINVAL), pathText);
    }
    FormatContextPtr formatContext(rawContext);

    // The image2 muxer treats a filename without a %d pattern as an image
    // sequence and refuses to overwrite a single file unless it is told
    // otherwise. Only muxers that define the option are configured.
    if (formatContext->priv_data != nullptr &&
        av_opt_find(formatContext->priv_data, "update", nullptr, 0, 0) != nullptr) {
        result = av_opt_set_int(formatContext->priv_data, "update", 1, 0);
        if (result < 0) {
            throwFfmpegError("Configuring single-image output", result, pathText);
        }
    }

    AVStream* const stream = avformat_new_stream(formatContext.get(), nullptr);
    if (stream == nullptr) {
        throw std::runtime_error("Cannot save '" + pathText + "': failed to allocate an output stream");
    }
    stream->time_base = codecContext->time_base;
    result = avcodec_parameters_from_context(stream->codecpar, codecContext.get());
    if (result < 0) {
        throwFfmpegError("Copying the stream parameters", result, pathText);
    }

    // From here on the output file can be created or truncated, so a failure
    // has to clean up after itself.
    outputGuard.arm();

    if ((formatContext->oformat->flags & AVFMT_NOFILE) == 0) {
        // The format context takes ownership of the I/O context; its deleter
        // closes it. Muxers flagged AVFMT_NOFILE (such as image2) open the
        // file themselves from the URL given to avformat_alloc_output_context2.
        result = avio_open(&formatContext->pb, pathText.c_str(), AVIO_FLAG_WRITE);
        if (result < 0) {
            throwFfmpegError("Opening the output file", result, pathText);
        }
    }

    result = avformat_write_header(formatContext.get(), nullptr);
    if (result < 0) {
        throwFfmpegError("Writing the output header", result, pathText);
    }

    // The muxer may have adjusted the stream's time base, so read it back
    // before timestamping packets.
    const AVRational streamTimeBase = stream->time_base;

    // ---- Encode ----------------------------------------------------------
    PacketPtr packet(av_packet_alloc());
    if (!packet) {
        throw std::runtime_error("Cannot save '" + pathText + "': failed to allocate a packet");
    }

    std::int64_t packetsWritten = 0;
    const auto drainEncoder = [&]() {
        for (;;) {
            const int receiveResult = avcodec_receive_packet(codecContext.get(), packet.get());
            if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) {
                return;
            }
            if (receiveResult < 0) {
                throwFfmpegError("Receiving an encoded packet", receiveResult, pathText);
            }
            av_packet_rescale_ts(packet.get(), codecContext->time_base, streamTimeBase);
            packet->stream_index = stream->index;
            // av_interleaved_write_frame() takes ownership of the packet and
            // blanks it, on failure as well as on success.
            const int writeResult = av_interleaved_write_frame(formatContext.get(), packet.get());
            if (writeResult < 0) {
                throwFfmpegError("Writing an encoded packet", writeResult, pathText);
            }
            ++packetsWritten;
        }
    };

    result = avcodec_send_frame(codecContext.get(), frame.get());
    if (result < 0) {
        throwFfmpegError("Submitting the frame to the encoder", result, pathText);
    }
    drainEncoder();

    // Flush: encoders are allowed to hold a frame back, so the leftovers have
    // to be drained or the file would be truncated.
    result = avcodec_send_frame(codecContext.get(), nullptr);
    if (result < 0 && result != AVERROR_EOF) {
        throwFfmpegError("Flushing the encoder", result, pathText);
    }
    drainEncoder();

    result = av_write_trailer(formatContext.get());
    if (result < 0) {
        throwFfmpegError("Finalising the output file", result, pathText);
    }

    // An encoder that emitted nothing would leave an empty file, which is not
    // something the caller should be told succeeded.
    if (packetsWritten == 0) {
        throw std::runtime_error("Encoding '" + pathText + "' produced no image data");
    }

    outputGuard.commit();
}

}  // namespace

// ============================================================
// ImageWrite
// ============================================================

bool ImageWrite::isFormatAvailable(const Format format) noexcept {
    const char* const codecName = codecNameFor(format);
    return codecName != nullptr && avcodec_find_encoder_by_name(codecName) != nullptr;
}

void ImageWrite::savePNG(const unsigned char* data, const int width, const int height,
                    const std::filesystem::path& outputPath, const PixelFormat format,
                    const int rowStrideBytes, const int compressionLevel) const {
    saveImage({.format = Format::PNG,
               .sourceFormat = format,
               .data = data,
               .width = width,
               .height = height,
               .rowStrideBytes = rowStrideBytes,
               .compressionLevel = compressionLevel,
               .quality = kEncoderDefault,
               .lossless = false},
              outputPath);
}

void ImageWrite::saveJPEG(const unsigned char* data, const int width, const int height,
                     const std::filesystem::path& outputPath, const PixelFormat format,
                     const int rowStrideBytes, const int quality) const {
    saveImage({.format = Format::JPEG,
               .sourceFormat = format,
               .data = data,
               .width = width,
               .height = height,
               .rowStrideBytes = rowStrideBytes,
               .compressionLevel = kEncoderDefault,
               .quality = quality,
               .lossless = false},
              outputPath);
}

void ImageWrite::saveBMP(const unsigned char* data, const int width, const int height,
                    const std::filesystem::path& outputPath, const PixelFormat format,
                    const int rowStrideBytes) const {
    saveImage({.format = Format::BMP,
               .sourceFormat = format,
               .data = data,
               .width = width,
               .height = height,
               .rowStrideBytes = rowStrideBytes,
               .compressionLevel = kEncoderDefault,
               .quality = kEncoderDefault,
               .lossless = false},
              outputPath);
}

void ImageWrite::saveWebP(const unsigned char* data, const int width, const int height,
                     const std::filesystem::path& outputPath, const PixelFormat format,
                     const int rowStrideBytes, const int quality, const bool lossless) const {
    saveImage({.format = Format::WebP,
               .sourceFormat = format,
               .data = data,
               .width = width,
               .height = height,
               .rowStrideBytes = rowStrideBytes,
               .compressionLevel = kEncoderDefault,
               .quality = quality,
               .lossless = lossless},
              outputPath);
}
