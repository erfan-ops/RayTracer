#include <Image.hpp>

#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace {

// ============================================================
// RAII wrappers for FFmpeg resources
// ============================================================

// As in ImageWrite.cpp, each deleter normalises the free function's calling
// convention so std::unique_ptr can release the resource on every path,
// including stack unwinding.

// Input format contexts are released with avformat_close_input(), which also
// closes the file and frees the streams; avformat_free_context() alone would
// leak the stream array and leave the file open.
struct InputFormatContextDeleter {
    void operator()(AVFormatContext* context) const noexcept {
        if (context != nullptr) {
            avformat_close_input(&context);
        }
    }
};

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

struct SwsContextDeleter {
    void operator()(SwsContext* context) const noexcept {
        if (context != nullptr) {
            sws_freeContext(context);
        }
    }
};

using InputFormatContextPtr = std::unique_ptr<AVFormatContext, InputFormatContextDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

// ============================================================
// Diagnostics
// ============================================================

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

[[nodiscard]] const char* pixelFormatName(const AVPixelFormat format) noexcept {
    const char* const name = av_get_pix_fmt_name(format);
    return name != nullptr ? name : "unknown";
}

/// Whether the decoder's pixel format carries chroma that swscale has to
/// expand. RGB formats already hold full-range samples; single-channel formats
/// have no chroma to scale. Only YUV-like formats benefit from an explicit
/// range hint.
[[nodiscard]] bool hasSeparateChroma(const AVPixelFormat format) noexcept {
    const AVPixFmtDescriptor* const description = av_pix_fmt_desc_get(format);
    return description != nullptr && (description->flags & AV_PIX_FMT_FLAG_RGB) == 0 &&
           description->nb_components >= 3;
}

// ============================================================
// Colour space
// ============================================================

/// Decoded 8-bit samples are assumed to carry the sRGB transfer function, the
/// conventional encoding for PNG and JPEG. The renderer shades in linear light,
/// so each possible byte value is converted once into a lookup table; pixel
/// access then costs a table read instead of a pow() call per channel.
///
/// There is deliberately no detection of inherently linear formats: callers of
/// this ray tracer use ordinary 8-bit sRGB images.
[[nodiscard]] const std::array<double, 256>& srgbToLinearTable() {
    static const std::array<double, 256> table = [] {
        std::array<double, 256> values = {};
        for (int value = 0; value < 256; ++value) {
            const double encoded = static_cast<double>(value) / 255.0;
            // IEC 61966-2-1 sRGB electro-optical transfer function.
            values[static_cast<std::size_t>(value)] =
                encoded <= 0.04045 ? encoded / 12.92
                                   : std::pow((encoded + 0.055) / 1.055, 2.4);
        }
        return values;
    }();
    return table;
}

// ============================================================
// Decoding pipeline
// ============================================================

/// A decoded pixel format together with the colour range that applies to it.
struct SourceFormat {
    AVPixelFormat format = AV_PIX_FMT_NONE;
    AVColorRange range = AVCOL_RANGE_UNSPECIFIED;
};

/// The deprecated yuvj* formats are identical to their plain yuv*
/// counterparts except that they imply full-range chroma. libavcodec still
/// emits them for JPEG, and libswscale warns when it receives one, so they are
/// mapped to the plain format here and the implied range is carried alongside
/// and applied to the scaler explicitly.
[[nodiscard]] SourceFormat normaliseSourceFormat(const AVPixelFormat format,
                                                 const AVColorRange range) noexcept {
    switch (format) {
        case AV_PIX_FMT_YUVJ420P: return {AV_PIX_FMT_YUV420P, AVCOL_RANGE_JPEG};
        case AV_PIX_FMT_YUVJ422P: return {AV_PIX_FMT_YUV422P, AVCOL_RANGE_JPEG};
        case AV_PIX_FMT_YUVJ444P: return {AV_PIX_FMT_YUV444P, AVCOL_RANGE_JPEG};
        case AV_PIX_FMT_YUVJ440P: return {AV_PIX_FMT_YUV440P, AVCOL_RANGE_JPEG};
        case AV_PIX_FMT_YUVJ411P: return {AV_PIX_FMT_YUV411P, AVCOL_RANGE_JPEG};
        default: return {format, range};
    }
}

/// A decoded image in the form the class stores it. Kept local to load() until
/// every step has succeeded, which is what gives load() its strong exception
/// guarantee.
struct DecodedImage {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels; ///< Tightly packed RGB24.
};

/// Picks the stream to decode. A still image carried by a container (cover art,
/// for example) is preferred over moving pictures, because this class is an
/// image reader; otherwise FFmpeg's best video stream is used.
[[nodiscard]] int selectStream(AVFormatContext* formatContext) noexcept {
    for (unsigned int index = 0; index < formatContext->nb_streams; ++index) {
        const AVStream* const stream = formatContext->streams[index];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) != 0) {
            return static_cast<int>(index);
        }
    }
    return av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
}

/// Reads packets from @p formatContext and decodes them until the first frame
/// arrives. @p frame receives that frame and stays owned by the caller.
void decodeFirstFrame(AVFormatContext* formatContext, AVCodecContext* decoderContext,
                      const int streamIndex, AVFrame* frame, const std::string& pathText) {
    PacketPtr packet(av_packet_alloc());
    if (!packet) {
        throw std::bad_alloc();
    }

    bool haveFrame = false;
    bool inputFinished = false;
    bool flushSent = false;
    bool packetPending = false;

    while (!haveFrame) {
        // Feed the decoder one packet, or the flush marker once the file is
        // exhausted.
        if (!inputFinished && !packetPending) {
            const int readResult = av_read_frame(formatContext, packet.get());
            if (readResult == AVERROR_EOF) {
                inputFinished = true;
            } else if (readResult < 0) {
                throwFfmpegError("Reading image data", readResult, pathText);
            } else if (packet->stream_index == streamIndex) {
                packetPending = true;
            } else {
                // Audio, subtitle and other streams are irrelevant to a single
                // still image.
                av_packet_unref(packet.get());
            }
        }

        if (packetPending) {
            const int sendResult = avcodec_send_packet(decoderContext, packet.get());
            if (sendResult == AVERROR(EAGAIN)) {
                // The decoder is full. The packet has not been consumed, so it
                // stays pending while receive_frame() drains the decoder.
            } else {
                if (sendResult < 0) {
                    throwFfmpegError("Decoding image data", sendResult, pathText);
                }
                av_packet_unref(packet.get());
                packetPending = false;
            }
        } else if (inputFinished && !flushSent) {
            // The API requires null packets to flush frames the decoder is
            // still holding back, which some codecs do until end of stream.
            const int flushResult = avcodec_send_packet(decoderContext, nullptr);
            if (flushResult == AVERROR(EAGAIN)) {
                // Buffered output must be drained before the flush marker is
                // accepted; receive_frame() below does that and the marker is
                // retried on the next iteration.
            } else {
                if (flushResult < 0 && flushResult != AVERROR_EOF) {
                    throwFfmpegError("Flushing the decoder", flushResult, pathText);
                }
                flushSent = true;
            }
        }

        const int receiveResult = avcodec_receive_frame(decoderContext, frame);
        if (receiveResult == 0) {
            haveFrame = true;
        } else if (receiveResult == AVERROR_EOF) {
            break;
        } else if (receiveResult != AVERROR(EAGAIN)) {
            throwFfmpegError("Decoding image data", receiveResult, pathText);
        } else if (inputFinished && !packetPending && flushSent) {
            // The decoder has accepted the flush and has no frame left.
            break;
        }
    }

    if (!haveFrame) {
        throw std::runtime_error("Cannot load '" + pathText +
                                 "': the file contains no decodable image frame");
    }
}

/// Converts @p frame to tightly packed RGB24. Dimensions come from the decoded
/// frame, not from the container metadata, because they can differ (cropping
/// and codec-side scaling included).
[[nodiscard]] DecodedImage convertFrame(const AVFrame* frame, const std::string& pathText) {
    const int width = frame->width;
    const int height = frame->height;
    if (width <= 0 || height <= 0) {
        std::ostringstream message;
        message << "Cannot load '" << pathText << "': the decoder produced an invalid size "
                << width << 'x' << height;
        throw std::runtime_error(message.str());
    }

    // Let libavcodec reject sizes whose row strides or total buffer size would
    // overflow the int-based API. This also guards the arithmetic below.
    int result = av_image_check_size(static_cast<unsigned int>(width),
                                     static_cast<unsigned int>(height), 0, nullptr);
    if (result < 0) {
        throwFfmpegError("Validating the decoded image size", result, pathText);
    }

    const AVPixelFormat decodedFormat = static_cast<AVPixelFormat>(frame->format);
    if (decodedFormat == AV_PIX_FMT_NONE) {
        throw std::runtime_error("Cannot load '" + pathText +
                                 "': the decoder produced no pixel format");
    }
    const SourceFormat source = normaliseSourceFormat(decodedFormat, frame->color_range);

    constexpr AVPixelFormat kDestinationFormat = AV_PIX_FMT_RGB24;

    int destinationStrides[4] = {};
    result = av_image_fill_linesizes(destinationStrides, kDestinationFormat, width);
    if (result < 0) {
        throwFfmpegError("Computing the output row stride", result, pathText);
    }

    const int bufferSize = av_image_get_buffer_size(kDestinationFormat, width, height, 1);
    if (bufferSize < 0) {
        throwFfmpegError("Computing the output buffer size", bufferSize, pathText);
    }

    std::vector<unsigned char> pixels(static_cast<std::size_t>(bufferSize));

    SwsContextPtr scaler(sws_getContext(width, height, source.format,
                                        width, height, kDestinationFormat, SWS_POINT,
                                        nullptr, nullptr, nullptr));
    if (!scaler) {
        throw std::runtime_error("Cannot load '" + pathText +
                                 "': failed to create a converter from " +
                                 pixelFormatName(source.format) + " to rgb24");
    }

    // swscale defaults to limited-range chroma. Files that declare full range
    // (JPEG and most still-image formats) would otherwise come out too dark
    // and desaturated, so mirror their range explicitly. RGB and grayscale
    // sources have no chroma range to configure. yuvj* formats arrive here as
    // their plain yuv* counterparts with the range already set to JPEG.
    if (hasSeparateChroma(source.format) && source.range == AVCOL_RANGE_JPEG) {
        const int* const coefficients = sws_getCoefficients(SWS_CS_DEFAULT);
        constexpr int kFullRange = 1;
        result = sws_setColorspaceDetails(scaler.get(), coefficients, kFullRange,
                                          coefficients, kFullRange, 0, 1 << 16, 1 << 16);
        if (result < 0) {
            throwFfmpegError("Configuring the input colour range", result, pathText);
        }
    }

    unsigned char* destinationPlanes[4] = {pixels.data(), nullptr, nullptr, nullptr};
    const int destinationPlaneStrides[4] = {destinationStrides[0], 0, 0, 0};
    const int convertedRows = sws_scale(scaler.get(), frame->data, frame->linesize, 0, height,
                                        destinationPlanes, destinationPlaneStrides);
    if (convertedRows < 0) {
        throwFfmpegError("Converting the decoded image to rgb24", convertedRows, pathText);
    }
    if (convertedRows != height) {
        std::ostringstream message;
        message << "Converting '" << pathText << "' to rgb24 produced " << convertedRows << " of "
                << height << " rows";
        throw std::runtime_error(message.str());
    }

    DecodedImage decoded;
    decoded.width = width;
    decoded.height = height;
    decoded.pixels = std::move(pixels);
    return decoded;
}

}  // namespace

// ============================================================
// ImageRead
// ============================================================

ImageRead::ImageRead() = default;

ImageRead::ImageRead(const std::filesystem::path& path) {
    load(path);
}

ImageRead::~ImageRead() = default;

ImageRead::ImageRead(const ImageRead& other) = default;

ImageRead::ImageRead(ImageRead&& other) noexcept
    : width_(other.width_), height_(other.height_), pixels_(std::move(other.pixels_)) {
    // The moved-from object must not look loaded: its pixel buffer is now
    // owned by this object and its size is no longer tied to its dimensions.
    other.width_ = 0;
    other.height_ = 0;
}

ImageRead& ImageRead::operator=(const ImageRead& other) = default;

ImageRead& ImageRead::operator=(ImageRead&& other) noexcept {
    if (this != &other) {
        pixels_ = std::move(other.pixels_);
        width_ = other.width_;
        height_ = other.height_;
        other.width_ = 0;
        other.height_ = 0;
    }
    return *this;
}

void ImageRead::load(const std::filesystem::path& path) {
    if (path.empty()) {
        throw std::invalid_argument("Cannot load an image from an empty path");
    }

    const std::string pathText = toUtf8String(path);

    // Checking the path first turns the most common mistakes into precise
    // messages instead of libavformat's generic "Invalid data found" text.
    std::error_code fileError;
    const bool exists = std::filesystem::exists(path, fileError);
    if (fileError) {
        throw std::runtime_error("Cannot load '" + pathText + "': " + fileError.message());
    }
    if (!exists) {
        throw std::runtime_error("Cannot load '" + pathText +
                                 "': the file does not exist or is not accessible");
    }
    const bool isDirectory = std::filesystem::is_directory(path, fileError);
    if (fileError) {
        throw std::runtime_error("Cannot load '" + pathText + "': " + fileError.message());
    }
    if (isDirectory) {
        throw std::runtime_error("Cannot load '" + pathText +
                                 "': the path is a directory, not an image file");
    }

    // Everything below works on locals. The object is only updated once a
    // complete image has been decoded, so a failed load leaves the previously
    // loaded image untouched.
    AVFormatContext* rawFormatContext = nullptr;
    int result = avformat_open_input(&rawFormatContext, pathText.c_str(), nullptr, nullptr);
    if (result < 0) {
        // Documented to leave the pointer null, but release defensively in
        // case a demuxer got far enough to allocate before failing.
        if (rawFormatContext != nullptr) {
            avformat_close_input(&rawFormatContext);
        }
        throwFfmpegError("Opening the input file", result, pathText);
    }
    InputFormatContextPtr formatContext(rawFormatContext);

    result = avformat_find_stream_info(formatContext.get(), nullptr);
    if (result < 0) {
        throwFfmpegError("Reading the stream information", result, pathText);
    }

    const int streamIndex = selectStream(formatContext.get());
    if (streamIndex < 0) {
        throw std::runtime_error("Cannot load '" + pathText +
                                 "': the file contains no image or video stream");
    }

    const AVCodecParameters* const parameters = formatContext->streams[streamIndex]->codecpar;
    const AVCodec* const decoder = avcodec_find_decoder(parameters->codec_id);
    if (decoder == nullptr) {
        throw std::runtime_error("Cannot load '" + pathText + "': this FFmpeg build has no "
                                 "decoder for codec '" +
                                 std::string(avcodec_get_name(parameters->codec_id)) + "'");
    }

    CodecContextPtr decoderContext(avcodec_alloc_context3(decoder));
    if (!decoderContext) {
        throw std::bad_alloc();
    }

    result = avcodec_parameters_to_context(decoderContext.get(), parameters);
    if (result < 0) {
        throwFfmpegError("Copying the decoder parameters", result, pathText);
    }

    result = avcodec_open2(decoderContext.get(), decoder, nullptr);
    if (result < 0) {
        throwFfmpegError("Opening the decoder", result, pathText);
    }

    FramePtr frame(av_frame_alloc());
    if (!frame) {
        throw std::bad_alloc();
    }

    decodeFirstFrame(formatContext.get(), decoderContext.get(), streamIndex, frame.get(), pathText);
    DecodedImage decoded = convertFrame(frame.get(), pathText);

    // Commit. These operations are non-throwing, so the strong guarantee holds
    // all the way to the end.
    pixels_.swap(decoded.pixels);
    width_ = decoded.width;
    height_ = decoded.height;
}

int ImageRead::width() const {
    requireLoaded();
    return width_;
}

int ImageRead::height() const {
    requireLoaded();
    return height_;
}

const Color ImageRead::pixel_data(const int x, const int y) const {
    requireLoaded();

    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        std::ostringstream message;
        message << "Pixel (" << x << ", " << y << ") is outside the loaded " << width_ << 'x'
                << height_ << " image";
        throw std::out_of_range(message.str());
    }

    const std::array<double, 256>& toLinear = srgbToLinearTable();
    const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                                static_cast<std::size_t>(x)) *
                               3;
    return Color(toLinear[pixels_[offset]], toLinear[pixels_[offset + 1]],
                 toLinear[pixels_[offset + 2]]);
}

void ImageRead::requireLoaded() const {
    if (width_ <= 0 || height_ <= 0 || pixels_.empty()) {
        throw std::logic_error("No image is loaded; call ImageRead::load() first");
    }
}
