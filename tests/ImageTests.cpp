// Tests for the Image encoder.
//
// Every written file is fed back through libavcodec/libavformat and compared
// against the source pixels, so "the file exists" is never taken as proof that
// encoding worked.
//
// Registered with CTest as RayTracer.Image; run it directly for the same result.

#include <Image.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace {

// ------------------------------------------------------------
// Tiny check harness
// ------------------------------------------------------------

int g_checks = 0;
int g_failures = 0;

void report(const bool passed, const std::string& description) {
    ++g_checks;
    if (!passed) {
        ++g_failures;
        std::cout << "  FAIL: " << description << '\n';
    }
}

#define CHECK(condition) report((condition), #condition)
#define CHECK_MESSAGE(condition, message) report((condition), (message))

void section(const char* name) {
    std::cout << "== " << name << '\n';
}

// ------------------------------------------------------------
// Decoding helpers
// ------------------------------------------------------------

struct FormatContextCloser {
    void operator()(AVFormatContext* context) const noexcept {
        if (context != nullptr) {
            avformat_close_input(&context);
        }
    }
};

struct CodecContextCloser {
    void operator()(AVCodecContext* context) const noexcept {
        if (context != nullptr) {
            avcodec_free_context(&context);
        }
    }
};

struct FrameCloser {
    void operator()(AVFrame* frame) const noexcept {
        if (frame != nullptr) {
            av_frame_free(&frame);
        }
    }
};

struct PacketCloser {
    void operator()(AVPacket* packet) const noexcept {
        if (packet != nullptr) {
            av_packet_free(&packet);
        }
    }
};

struct SwsCloser {
    void operator()(SwsContext* context) const noexcept {
        if (context != nullptr) {
            sws_freeContext(context);
        }
    }
};

struct DecodedImage {
    int width = 0;
    int height = 0;
    AVPixelFormat sourceFormat = AV_PIX_FMT_NONE;
    std::string codecName;
    std::vector<unsigned char> pixels; // in the requested target format, tightly packed
};

std::string ffmpegErrorText(const int errorCode) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    if (av_strerror(errorCode, buffer, sizeof(buffer)) < 0) {
        return "error " + std::to_string(errorCode);
    }
    return std::string(buffer);
}

/// FFmpeg's file protocol expects UTF-8 on Windows.
std::string toUtf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

/// Decodes the first video frame of @p path into @p targetFormat (RGB24 or
/// RGBA). Returns false and fills @p error on failure.
bool decodeImage(const std::filesystem::path& path, const AVPixelFormat targetFormat,
                 DecodedImage& decoded, std::string& error) {
    AVFormatContext* rawFormatContext = nullptr;
    int result =
        avformat_open_input(&rawFormatContext, toUtf8(path).c_str(), nullptr, nullptr);
    if (result < 0) {
        error = "avformat_open_input: " + ffmpegErrorText(result);
        return false;
    }
    std::unique_ptr<AVFormatContext, FormatContextCloser> formatContext(rawFormatContext);

    result = avformat_find_stream_info(formatContext.get(), nullptr);
    if (result < 0) {
        error = "avformat_find_stream_info: " + ffmpegErrorText(result);
        return false;
    }

    const int streamIndex =
        av_find_best_stream(formatContext.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        error = "no video stream";
        return false;
    }

    const AVCodecParameters* parameters = formatContext->streams[streamIndex]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(parameters->codec_id);
    if (decoder == nullptr) {
        error = "no decoder for codec id " + std::to_string(parameters->codec_id);
        return false;
    }
    decoded.codecName = decoder->name;

    std::unique_ptr<AVCodecContext, CodecContextCloser> decoderContext(avcodec_alloc_context3(decoder));
    std::unique_ptr<AVFrame, FrameCloser> frame(av_frame_alloc());
    std::unique_ptr<AVPacket, PacketCloser> packet(av_packet_alloc());
    if (!decoderContext || !frame || !packet) {
        error = "decoder allocation failed";
        return false;
    }

    result = avcodec_parameters_to_context(decoderContext.get(), parameters);
    if (result < 0) {
        error = "avcodec_parameters_to_context: " + ffmpegErrorText(result);
        return false;
    }
    result = avcodec_open2(decoderContext.get(), decoder, nullptr);
    if (result < 0) {
        error = "avcodec_open2: " + ffmpegErrorText(result);
        return false;
    }

    bool haveFrame = false;
    while (!haveFrame) {
        result = av_read_frame(formatContext.get(), packet.get());
        if (result < 0) {
            error = "no decodable frame in the file";
            return false;
        }
        if (packet->stream_index != streamIndex) {
            av_packet_unref(packet.get());
            continue;
        }
        const int sendResult = avcodec_send_packet(decoderContext.get(), packet.get());
        av_packet_unref(packet.get());
        if (sendResult < 0) {
            continue;
        }
        haveFrame = avcodec_receive_frame(decoderContext.get(), frame.get()) >= 0;
    }

    decoded.width = frame->width;
    decoded.height = frame->height;
    decoded.sourceFormat = static_cast<AVPixelFormat>(frame->format);

    const AVPixelFormat destination =
        targetFormat == AV_PIX_FMT_RGBA ? AV_PIX_FMT_RGBA : AV_PIX_FMT_RGB24;
    const int destinationStride =
        destination == AV_PIX_FMT_RGBA ? decoded.width * 4 : decoded.width * 3;
    decoded.pixels.assign(static_cast<std::size_t>(destinationStride) * decoded.height, 0);

    std::unique_ptr<SwsContext, SwsCloser> scaler(
        sws_getContext(decoded.width, decoded.height, static_cast<AVPixelFormat>(frame->format),
                       decoded.width, decoded.height, destination, SWS_POINT, nullptr, nullptr, nullptr));
    if (!scaler) {
        error = "sws_getContext failed";
        return false;
    }
    // Reproduce the full-range convention the file was encoded with, so the
    // round trip is comparable with the original pixels.
    if (frame->color_range == AVCOL_RANGE_JPEG) {
        const int* const coefficients = sws_getCoefficients(SWS_CS_DEFAULT);
        constexpr int kFullRange = 1;
        sws_setColorspaceDetails(scaler.get(), coefficients, kFullRange, coefficients, kFullRange, 0,
                                 1 << 16, 1 << 16);
    }

    unsigned char* destinationPlanes[4] = {decoded.pixels.data(), nullptr, nullptr, nullptr};
    const int destinationStrides[4] = {destinationStride, 0, 0, 0};
    const int converted =
        sws_scale(scaler.get(), (const std::uint8_t* const*)frame->data, frame->linesize, 0,
                  decoded.height, destinationPlanes, destinationStrides);
    if (converted != decoded.height) {
        error = "sws_scale produced " + std::to_string(converted) + " of " +
                std::to_string(decoded.height) + " rows";
        return false;
    }
    return true;
}

// ------------------------------------------------------------
// Comparison helpers
// ------------------------------------------------------------

/// Mean absolute per-channel difference; 0 means identical buffers.
float meanAbsoluteError(const std::vector<unsigned char>& lhs,
                         const std::vector<unsigned char>& rhs) {
    if (lhs.size() != rhs.size() || lhs.empty()) {
        return 1.0e9;
    }
    float total = 0.0;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        total += std::abs(static_cast<int>(lhs[i]) - static_cast<int>(rhs[i]));
    }
    return total / static_cast<float>(lhs.size());
}

/// Mean colour of one rectangular region, used to confirm that a decoded image
/// still shows the same large-scale structure as the source.
struct Rgb {
    float r = 0.0;
    float g = 0.0;
    float b = 0.0;
};

Rgb regionMean(const std::vector<unsigned char>& rgb, const int width, const int x0, const int y0,
               const int x1, const int y1) {
    Rgb mean;
    float count = 0.0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 3;
            mean.r += rgb[index];
            mean.g += rgb[index + 1];
            mean.b += rgb[index + 2];
            count += 1.0;
        }
    }
    if (count > 0.0) {
        mean.r /= count;
        mean.g /= count;
        mean.b /= count;
    }
    return mean;
}

float channelDistance(const Rgb& lhs, const Rgb& rhs) {
    return std::max({std::abs(lhs.r - rhs.r), std::abs(lhs.g - rhs.g), std::abs(lhs.b - rhs.b)});
}

std::uintmax_t fileSize(const std::filesystem::path& path) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    return error ? 0 : size;
}

std::string readAll(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

// ------------------------------------------------------------
// Test data
// ------------------------------------------------------------

constexpr int kWidth = 96;
constexpr int kHeight = 64;

/// A gradient with three strongly coloured quadrants, so that both fine detail
/// and large-scale colour survive a lossy round trip recognisably.
std::vector<unsigned char> makeRgbPattern(const int width, const int height) {
    std::vector<unsigned char> image(static_cast<std::size_t>(width) * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 3;
            const bool rightHalf = x >= width / 2;
            const bool bottomHalf = y >= height / 2;
            unsigned char r = static_cast<unsigned char>(x * 255 / (width - 1));
            unsigned char g = static_cast<unsigned char>(y * 255 / (height - 1));
            unsigned char b = static_cast<unsigned char>(((x ^ y) * 255) / width);
            if (rightHalf && !bottomHalf) {
                r = 220;
                g = 40;
                b = 40;
            } else if (!rightHalf && bottomHalf) {
                r = 30;
                g = 60;
                b = 210;
            } else if (rightHalf && bottomHalf) {
                r = 250;
                g = 230;
                b = 20;
            }
            image[index] = r;
            image[index + 1] = g;
            image[index + 2] = b;
        }
    }
    return image;
}

std::vector<unsigned char> makeRgbaPattern(const int width, const int height) {
    const std::vector<unsigned char> rgb = makeRgbPattern(width, height);
    std::vector<unsigned char> rgba(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width) * height; ++pixel) {
        rgba[pixel * 4 + 0] = rgb[pixel * 3 + 0];
        rgba[pixel * 4 + 1] = rgb[pixel * 3 + 1];
        rgba[pixel * 4 + 2] = rgb[pixel * 3 + 2];
        // A varying alpha makes a dropped or ignored alpha channel visible.
        rgba[pixel * 4 + 3] = static_cast<unsigned char>(64 + pixel % 192);
    }
    return rgba;
}

std::vector<unsigned char> makeGrayPattern(const int width, const int height) {
    std::vector<unsigned char> image(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image[static_cast<std::size_t>(y) * width + x] =
                static_cast<unsigned char>((x * 3 + y * 5) & 0xff);
        }
    }
    return image;
}

}  // namespace

int main() {
    const std::filesystem::path outputDir =
        std::filesystem::temp_directory_path() / "RayTracerImageTests";
    std::error_code error;
    std::filesystem::remove_all(outputDir, error);
    std::filesystem::create_directories(outputDir, error);
    if (error) {
        std::cout << "Could not create the test output directory: " << error.message() << '\n';
        return 1;
    }

    const Image image;
    const std::vector<unsigned char> rgb = makeRgbPattern(kWidth, kHeight);

    // --------------------------------------------------------
    section("Encoder availability");
    // --------------------------------------------------------
    const bool pngAvailable = Image::isFormatAvailable(Image::Format::PNG);
    const bool jpegAvailable = Image::isFormatAvailable(Image::Format::JPEG);
    const bool bmpAvailable = Image::isFormatAvailable(Image::Format::BMP);
    const bool webpAvailable = Image::isFormatAvailable(Image::Format::WebP);
    std::cout << "  PNG=" << pngAvailable << " JPEG=" << jpegAvailable << " BMP=" << bmpAvailable
              << " WebP=" << webpAvailable << '\n';
    CHECK_MESSAGE(pngAvailable, "this FFmpeg build must provide the PNG encoder");
    CHECK_MESSAGE(jpegAvailable, "this FFmpeg build must provide the MJPEG encoder");
    CHECK_MESSAGE(bmpAvailable, "this FFmpeg build must provide the BMP encoder");

    // --------------------------------------------------------
    section("PNG round trip (RGB24, exact)");
    // --------------------------------------------------------
    {
        const std::filesystem::path path = outputDir / "rgb.png";
        image.savePNG(rgb.data(), kWidth, kHeight, path);
        DecodedImage decoded;
        std::string decodeError;
        CHECK_MESSAGE(decodeImage(path, AV_PIX_FMT_RGB24, decoded, decodeError),
                      "PNG must decode back: " + decodeError);
        CHECK(decoded.width == kWidth);
        CHECK(decoded.height == kHeight);
        CHECK_MESSAGE(decoded.codecName == "png", "file must really be PNG, got " + decoded.codecName);
        CHECK_MESSAGE(meanAbsoluteError(decoded.pixels, rgb) == 0.0, "PNG must be pixel exact");
    }

    // --------------------------------------------------------
    section("PNG alpha (RGBA32, exact)");
    // --------------------------------------------------------
    {
        const std::vector<unsigned char> rgba = makeRgbaPattern(kWidth, kHeight);
        const std::filesystem::path path = outputDir / "rgba.png";
        image.savePNG(rgba.data(), kWidth, kHeight, path, Image::PixelFormat::RGBA32);
        DecodedImage decoded;
        std::string decodeError;
        CHECK_MESSAGE(decodeImage(path, AV_PIX_FMT_RGBA, decoded, decodeError),
                      "RGBA PNG must decode back: " + decodeError);
        CHECK(decoded.width == kWidth && decoded.height == kHeight);
        CHECK_MESSAGE(meanAbsoluteError(decoded.pixels, rgba) == 0.0,
                      "RGBA PNG must preserve every channel including alpha");
    }

    // --------------------------------------------------------
    section("PNG grayscale (GRAY8, exact)");
    // --------------------------------------------------------
    {
        const std::vector<unsigned char> gray = makeGrayPattern(kWidth, kHeight);
        const std::filesystem::path path = outputDir / "gray.png";
        image.savePNG(gray.data(), kWidth, kHeight, path, Image::PixelFormat::GRAY8);
        DecodedImage decoded;
        std::string decodeError;
        CHECK_MESSAGE(decodeImage(path, AV_PIX_FMT_RGB24, decoded, decodeError),
                      "grayscale PNG must decode back: " + decodeError);
        CHECK(decoded.width == kWidth && decoded.height == kHeight);
        // Each gray sample must come back as an equal r == g == b triple.
        bool grayConsistent = true;
        for (std::size_t pixel = 0; pixel < gray.size() && grayConsistent; ++pixel) {
            grayConsistent = decoded.pixels[pixel * 3 + 0] == gray[pixel] &&
                             decoded.pixels[pixel * 3 + 1] == gray[pixel] &&
                             decoded.pixels[pixel * 3 + 2] == gray[pixel];
        }
        CHECK_MESSAGE(grayConsistent, "grayscale samples must round trip unchanged");
    }

    // --------------------------------------------------------
    section("PNG compression level");
    // --------------------------------------------------------
    {
        const std::filesystem::path stored = outputDir / "level0.png";
        const std::filesystem::path packed = outputDir / "level9.png";
        image.savePNG(rgb.data(), kWidth, kHeight, stored, Image::PixelFormat::RGB24, 0, 0);
        image.savePNG(rgb.data(), kWidth, kHeight, packed, Image::PixelFormat::RGB24, 0, 9);
        std::cout << "  level 0 = " << fileSize(stored) << " bytes, level 9 = " << fileSize(packed)
                  << " bytes\n";
        CHECK_MESSAGE(fileSize(stored) > fileSize(packed),
                      "compression level 0 must produce a larger file than level 9");

        DecodedImage decoded;
        std::string decodeError;
        CHECK_MESSAGE(decodeImage(packed, AV_PIX_FMT_RGB24, decoded, decodeError),
                      "level 9 PNG must decode back: " + decodeError);
        CHECK_MESSAGE(meanAbsoluteError(decoded.pixels, rgb) == 0.0,
                      "compression must not change pixels");
    }

    // --------------------------------------------------------
    section("JPEG quality control");
    // --------------------------------------------------------
    if (jpegAvailable) {
        const std::filesystem::path best = outputDir / "q95.jpg";
        const std::filesystem::path worst = outputDir / "q10.jpg";
        image.saveJPEG(rgb.data(), kWidth, kHeight, best, Image::PixelFormat::RGB24, 0, 95);
        image.saveJPEG(rgb.data(), kWidth, kHeight, worst, Image::PixelFormat::RGB24, 0, 10);
        std::cout << "  quality 95 = " << fileSize(best) << " bytes, quality 10 = "
                  << fileSize(worst) << " bytes\n";
        CHECK_MESSAGE(fileSize(best) > fileSize(worst),
                      "JPEG quality must change the encoded size (high quality must be larger)");

        DecodedImage bestDecoded;
        DecodedImage worstDecoded;
        std::string decodeError;
        const bool bestOk = decodeImage(best, AV_PIX_FMT_RGB24, bestDecoded, decodeError);
        CHECK_MESSAGE(bestOk, "JPEG must decode back: " + decodeError);
        const bool worstOk = decodeImage(worst, AV_PIX_FMT_RGB24, worstDecoded, decodeError);
        CHECK_MESSAGE(worstOk, "JPEG must decode back: " + decodeError);
        if (bestOk && worstOk) {
            CHECK(bestDecoded.width == kWidth && bestDecoded.height == kHeight);
            CHECK_MESSAGE(bestDecoded.codecName == "mjpeg",
                          "file must really be JPEG, got " + bestDecoded.codecName);
            const float bestError = meanAbsoluteError(bestDecoded.pixels, rgb);
            const float worstError = meanAbsoluteError(worstDecoded.pixels, rgb);
            std::cout << "  MAE at quality 95 = " << bestError << ", at quality 10 = " << worstError
                      << '\n';
            CHECK_MESSAGE(bestError < 12.0, "quality 95 must stay close to the source");
            CHECK_MESSAGE(worstError > bestError, "lower quality must deviate more from the source");

            // Large-scale structure must survive compression, which is what
            // makes the result recognisably the same image.
            const Rgb sourceTopLeft = regionMean(rgb, kWidth, 4, 4, kWidth / 2 - 4, kHeight / 2 - 4);
            const Rgb decodedTopLeft =
                regionMean(bestDecoded.pixels, kWidth, 4, 4, kWidth / 2 - 4, kHeight / 2 - 4);
            CHECK_MESSAGE(channelDistance(sourceTopLeft, decodedTopLeft) < 12.0,
                          "top-left quadrant must keep its colour through JPEG");
        }

        // The ends of the accepted quality range must encode, not just the
        // middle of it.
        for (const int boundary : {1, 100}) {
            const std::filesystem::path path =
                outputDir / ("q" + std::to_string(boundary) + ".jpg");
            bool wrote = true;
            try {
                image.saveJPEG(rgb.data(), kWidth, kHeight, path, Image::PixelFormat::RGB24, 0,
                               boundary);
            } catch (const std::exception& exception) {
                wrote = false;
                std::cout << "  quality " << boundary << " rejected: " << exception.what() << '\n';
            }
            report(wrote, "quality " + std::to_string(boundary) + " must be accepted");
            if (wrote) {
                DecodedImage boundaryDecoded;
                std::string decodeError;
                CHECK_MESSAGE(decodeImage(path, AV_PIX_FMT_RGB24, boundaryDecoded, decodeError),
                              "quality " + std::to_string(boundary) + " must decode: " + decodeError);
                CHECK(boundaryDecoded.width == kWidth && boundaryDecoded.height == kHeight);
            }
        }
    }

    // --------------------------------------------------------
    section("BMP round trip (exact)");
    // --------------------------------------------------------
    if (bmpAvailable) {
        const std::filesystem::path path = outputDir / "rgb.bmp";
        image.saveBMP(rgb.data(), kWidth, kHeight, path);
        DecodedImage decoded;
        std::string decodeError;
        CHECK_MESSAGE(decodeImage(path, AV_PIX_FMT_RGB24, decoded, decodeError),
                      "BMP must decode back: " + decodeError);
        CHECK(decoded.width == kWidth && decoded.height == kHeight);
        CHECK_MESSAGE(decoded.codecName == "bmp", "file must really be BMP, got " + decoded.codecName);
        CHECK_MESSAGE(meanAbsoluteError(decoded.pixels, rgb) == 0.0, "BMP must be pixel exact");
    }

    // --------------------------------------------------------
    section("WebP");
    // --------------------------------------------------------
    {
        const std::filesystem::path lossy = outputDir / "lossy.webp";
        const std::filesystem::path lossless = outputDir / "lossless.webp";
        bool lossyWrote = true;
        bool losslessWrote = true;
        try {
            image.saveWebP(rgb.data(), kWidth, kHeight, lossy, Image::PixelFormat::RGB24, 0, 90,
                           false);
        } catch (const std::exception& exception) {
            lossyWrote = false;
            std::cout << "  lossy WebP unavailable: " << exception.what() << '\n';
        }
        try {
            image.saveWebP(rgb.data(), kWidth, kHeight, lossless, Image::PixelFormat::RGB24, 0, 90,
                           true);
        } catch (const std::exception& exception) {
            losslessWrote = false;
            std::cout << "  lossless WebP unavailable: " << exception.what() << '\n';
        }
        // What isFormatAvailable() promises must match what saveWebP() does.
        CHECK_MESSAGE(webpAvailable == (lossyWrote && losslessWrote),
                      "isFormatAvailable(WebP) disagrees with what saveWebP actually does");

        if (lossyWrote) {
            DecodedImage decoded;
            std::string decodeError;
            CHECK_MESSAGE(decodeImage(lossy, AV_PIX_FMT_RGB24, decoded, decodeError),
                          "lossy WebP must decode back: " + decodeError);
            CHECK(decoded.width == kWidth && decoded.height == kHeight);
            CHECK_MESSAGE(decoded.codecName == "webp",
                          "file must really be WebP, got " + decoded.codecName);
            CHECK_MESSAGE(meanAbsoluteError(decoded.pixels, rgb) < 12.0,
                          "lossy WebP must stay close to the source");
        }
        if (losslessWrote) {
            DecodedImage decoded;
            std::string decodeError;
            CHECK_MESSAGE(decodeImage(lossless, AV_PIX_FMT_RGB24, decoded, decodeError),
                          "lossless WebP must decode back: " + decodeError);
            CHECK_MESSAGE(meanAbsoluteError(decoded.pixels, rgb) == 0.0,
                          "lossless WebP must be pixel exact");

            // The header promises alpha survives lossless WebP; check it.
            const std::vector<unsigned char> rgba = makeRgbaPattern(kWidth, kHeight);
            const std::filesystem::path alphaPath = outputDir / "alpha.webp";
            image.saveWebP(rgba.data(), kWidth, kHeight, alphaPath, Image::PixelFormat::RGBA32, 0, 90,
                           true);
            DecodedImage alphaDecoded;
            CHECK_MESSAGE(decodeImage(alphaPath, AV_PIX_FMT_RGBA, alphaDecoded, decodeError),
                          "lossless RGBA WebP must decode back: " + decodeError);
            CHECK_MESSAGE(meanAbsoluteError(alphaDecoded.pixels, rgba) == 0.0,
                          "lossless WebP must preserve the alpha channel");
        }
    }

    // --------------------------------------------------------
    section("Row stride handling");
    // --------------------------------------------------------
    {
        // Rows padded to a wider stride than the pixels need; the encoder must
        // honour the stride rather than assume tightly packed rows.
        constexpr int kPadding = 13;
        const int stride = kWidth * 3 + kPadding;
        std::vector<unsigned char> padded(static_cast<std::size_t>(stride) * kHeight, 0xAB);
        for (int y = 0; y < kHeight; ++y) {
            std::copy_n(rgb.begin() + static_cast<std::size_t>(y) * kWidth * 3, kWidth * 3,
                        padded.begin() + static_cast<std::size_t>(y) * stride);
        }

        const std::filesystem::path path = outputDir / "strided.png";
        image.savePNG(padded.data(), kWidth, kHeight, path, Image::PixelFormat::RGB24, stride);
        DecodedImage decoded;
        std::string decodeError;
        CHECK_MESSAGE(decodeImage(path, AV_PIX_FMT_RGB24, decoded, decodeError),
                      "strided PNG must decode back: " + decodeError);
        CHECK_MESSAGE(meanAbsoluteError(decoded.pixels, rgb) == 0.0,
                      "padding bytes must not leak into the image");
    }

    // --------------------------------------------------------
    section("Requested format wins over the file extension");
    // --------------------------------------------------------
    {
        const std::filesystem::path pngNamedJpeg = outputDir / "actually_png.jpg";
        image.savePNG(rgb.data(), kWidth, kHeight, pngNamedJpeg);
        DecodedImage decoded;
        std::string decodeError;
        CHECK_MESSAGE(decodeImage(pngNamedJpeg, AV_PIX_FMT_RGB24, decoded, decodeError),
                      "PNG written to a .jpg name must decode: " + decodeError);
        CHECK_MESSAGE(decoded.codecName == "png",
                      "savePNG must not switch to JPEG because of the extension, got " +
                          decoded.codecName);
        CHECK_MESSAGE(meanAbsoluteError(decoded.pixels, rgb) == 0.0,
                      "PNG written to a .jpg name must still be lossless");

        if (jpegAvailable) {
            const std::filesystem::path jpegNamedPng = outputDir / "actually_jpeg.png";
            image.saveJPEG(rgb.data(), kWidth, kHeight, jpegNamedPng);
            DecodedImage jpegDecoded;
            CHECK_MESSAGE(decodeImage(jpegNamedPng, AV_PIX_FMT_RGB24, jpegDecoded, decodeError),
                          "JPEG written to a .png name must decode: " + decodeError);
            CHECK_MESSAGE(jpegDecoded.codecName == "mjpeg",
                          "saveJPEG must not switch to PNG because of the extension, got " +
                              jpegDecoded.codecName);
        }
    }

    // --------------------------------------------------------
    section("File names are written literally, not as patterns");
    // --------------------------------------------------------
    {
        // The image2 muxer reads '%' as the start of an image-sequence pattern
        // unless it is told that a single image is being written, in which case
        // it would write "literal000.png" instead of the requested name.
        const std::filesystem::path path = outputDir / "literal%03d.png";
        image.savePNG(rgb.data(), kWidth, kHeight, path);
        CHECK_MESSAGE(std::filesystem::exists(path),
                      "a '%' in the file name must be written literally");

        DecodedImage decoded;
        std::string decodeError;
        CHECK_MESSAGE(decodeImage(path, AV_PIX_FMT_RGB24, decoded, decodeError),
                      "the literally named file must decode: " + decodeError);
        CHECK_MESSAGE(meanAbsoluteError(decoded.pixels, rgb) == 0.0,
                      "the literally named file must hold the right pixels");
    }

    // --------------------------------------------------------
    section("Repeated saves are stable and stateless");
    // --------------------------------------------------------
    {
        const std::filesystem::path first = outputDir / "repeat_0.png";
        image.savePNG(rgb.data(), kWidth, kHeight, first);
        const std::string reference = readAll(first);
        CHECK(!reference.empty());

        bool identical = true;
        for (int iteration = 1; iteration < 25 && identical; ++iteration) {
            const std::filesystem::path path =
                outputDir / ("repeat_" + std::to_string(iteration) + ".png");
            image.savePNG(rgb.data(), kWidth, kHeight, path);
            identical = readAll(path) == reference;
        }
        CHECK_MESSAGE(identical, "repeated saves of the same input must produce identical files");

        // Interleaving other formats must not disturb a later PNG.
        if (bmpAvailable) {
            image.saveBMP(rgb.data(), kWidth, kHeight, outputDir / "interleaved.bmp");
        }
        if (jpegAvailable) {
            image.saveJPEG(rgb.data(), kWidth, kHeight, outputDir / "interleaved.jpg");
        }
        const std::filesystem::path after = outputDir / "repeat_after.png";
        image.savePNG(rgb.data(), kWidth, kHeight, after);
        CHECK_MESSAGE(readAll(after) == reference,
                      "encoding other formats must not change later PNG output");
    }

    // --------------------------------------------------------
    section("Invalid arguments are rejected");
    // --------------------------------------------------------
    {
        const std::filesystem::path path = outputDir / "invalid.png";
        const auto expectInvalid = [&](const char* description, const auto& operation) {
            bool threw = false;
            std::string message;
            try {
                operation();
            } catch (const std::invalid_argument& exception) {
                threw = true;
            } catch (const std::exception& exception) {
                message = std::string("wrong exception type: ") + exception.what();
            }
            report(threw, std::string(description) + (threw ? "" : " -- " + message));
        };

        expectInvalid("null pixel buffer",
                      [&] { image.savePNG(nullptr, kWidth, kHeight, path); });
        expectInvalid("zero width", [&] { image.savePNG(rgb.data(), 0, kHeight, path); });
        expectInvalid("negative width", [&] { image.savePNG(rgb.data(), -7, kHeight, path); });
        expectInvalid("zero height", [&] { image.savePNG(rgb.data(), kWidth, 0, path); });
        expectInvalid("negative height", [&] { image.saveJPEG(rgb.data(), kWidth, -3, path); });
        expectInvalid("stride smaller than a packed row", [&] {
            image.savePNG(rgb.data(), kWidth, kHeight, path, Image::PixelFormat::RGB24, kWidth * 3 - 1);
        });
        expectInvalid("negative stride", [&] {
            image.savePNG(rgb.data(), kWidth, kHeight, path, Image::PixelFormat::RGB24, -(kWidth * 3));
        });
        expectInvalid("compression level above 9", [&] {
            image.savePNG(rgb.data(), kWidth, kHeight, path, Image::PixelFormat::RGB24, 0, 10);
        });
        expectInvalid("compression level below -1", [&] {
            image.savePNG(rgb.data(), kWidth, kHeight, path, Image::PixelFormat::RGB24, 0, -2);
        });
        expectInvalid("quality above 100", [&] {
            image.saveJPEG(rgb.data(), kWidth, kHeight, path, Image::PixelFormat::RGB24, 0, 101);
        });
        expectInvalid("quality of zero", [&] {
            image.saveJPEG(rgb.data(), kWidth, kHeight, path, Image::PixelFormat::RGB24, 0, 0);
        });
        expectInvalid("dimensions huge enough to overflow a size calculation", [&] {
            image.savePNG(rgb.data(), 1 << 30, 1 << 30, path);
        });

        // None of the rejected calls may have produced a file.
        CHECK_MESSAGE(!std::filesystem::exists(path),
                      "a rejected save must not leave an output file behind");

        // A rejected call must also leave an existing file at that path alone,
        // rather than deleting it as part of a clean-up that never started.
        const std::filesystem::path existing = outputDir / "preexisting.png";
        image.savePNG(rgb.data(), kWidth, kHeight, existing);
        const std::string untouched = readAll(existing);
        CHECK(!untouched.empty());
        try {
            image.savePNG(nullptr, kWidth, kHeight, existing);
        } catch (const std::invalid_argument&) {
        }
        CHECK_MESSAGE(readAll(existing) == untouched,
                      "a rejected save must not damage a pre-existing file");
    }

    // --------------------------------------------------------
    section("Write failures are reported and cleaned up");
    // --------------------------------------------------------
    {
        const std::filesystem::path missingDirectory = outputDir / "no_such_directory" / "out.png";
        bool threw = false;
        try {
            image.savePNG(rgb.data(), kWidth, kHeight, missingDirectory);
        } catch (const std::runtime_error& exception) {
            threw = true;
            std::cout << "  " << exception.what() << '\n';
        }
        CHECK_MESSAGE(threw, "writing into a missing directory must raise std::runtime_error");
        CHECK_MESSAGE(!std::filesystem::exists(missingDirectory),
                      "a failed write must not leave a partial file behind");

        // A directory used as the output path must fail too, and the clean-up
        // path must not delete it.
        bool directoryThrew = false;
        try {
            image.savePNG(rgb.data(), kWidth, kHeight, outputDir);
        } catch (const std::runtime_error& exception) {
            directoryThrew = true;
            std::cout << "  " << exception.what() << '\n';
        } catch (const std::exception&) {
        }
        CHECK_MESSAGE(directoryThrew, "using a directory as the output path must fail");
        CHECK_MESSAGE(std::filesystem::is_directory(outputDir),
                      "clean-up must not delete a directory that was passed by mistake");

        // An *empty* directory is the sharper case: std::filesystem::remove()
        // would happily delete one, so clean-up has to check the file type.
        const std::filesystem::path emptyDirectory = outputDir / "empty_directory";
        std::error_code directoryError;
        std::filesystem::create_directories(emptyDirectory, directoryError);
        bool emptyDirectoryThrew = false;
        try {
            image.savePNG(rgb.data(), kWidth, kHeight, emptyDirectory);
        } catch (const std::exception&) {
            emptyDirectoryThrew = true;
        }
        CHECK_MESSAGE(emptyDirectoryThrew, "using an empty directory as the output path must fail");
        CHECK_MESSAGE(std::filesystem::exists(emptyDirectory),
                      "clean-up must not delete an empty directory passed by mistake");

        // Failures must not accumulate state: repeat them, then confirm a good
        // save still works and is still exact.
        int failures = 0;
        for (int iteration = 0; iteration < 50; ++iteration) {
            try {
                image.savePNG(rgb.data(), kWidth, kHeight,
                              outputDir / "missing" / (std::to_string(iteration) + ".png"));
            } catch (const std::runtime_error&) {
                ++failures;
            }
        }
        CHECK_MESSAGE(failures == 50, "every failing save must raise, not just the first");

        const std::filesystem::path recovered = outputDir / "after_failures.png";
        image.savePNG(rgb.data(), kWidth, kHeight, recovered);
        DecodedImage decoded;
        std::string decodeError;
        CHECK_MESSAGE(decodeImage(recovered, AV_PIX_FMT_RGB24, decoded, decodeError),
                      "a save after repeated failures must still work: " + decodeError);
        CHECK_MESSAGE(meanAbsoluteError(decoded.pixels, rgb) == 0.0,
                      "a save after repeated failures must still be exact");
    }

    // --------------------------------------------------------
    section("Overwriting an existing file");
    // --------------------------------------------------------
    {
        const std::filesystem::path path = outputDir / "overwrite.png";
        image.savePNG(rgb.data(), kWidth, kHeight, path);
        const std::string first = readAll(path);

        const std::vector<unsigned char> gray = makeGrayPattern(kWidth, kHeight);
        std::vector<unsigned char> grayRgb(static_cast<std::size_t>(kWidth) * kHeight * 3);
        for (std::size_t pixel = 0; pixel < gray.size(); ++pixel) {
            grayRgb[pixel * 3 + 0] = gray[pixel];
            grayRgb[pixel * 3 + 1] = gray[pixel];
            grayRgb[pixel * 3 + 2] = gray[pixel];
        }
        image.savePNG(grayRgb.data(), kWidth, kHeight, path);
        CHECK_MESSAGE(readAll(path) != first, "an existing file must actually be replaced");

        DecodedImage decoded;
        std::string decodeError;
        CHECK_MESSAGE(decodeImage(path, AV_PIX_FMT_RGB24, decoded, decodeError),
                      "the replaced file must decode: " + decodeError);
        CHECK_MESSAGE(meanAbsoluteError(decoded.pixels, grayRgb) == 0.0,
                      "the replaced file must hold the new pixels");
    }

    // --------------------------------------------------------
    section("Copyable and movable");
    // --------------------------------------------------------
    {
        static_assert(std::is_copy_constructible_v<Image>);
        static_assert(std::is_copy_assignable_v<Image>);
        static_assert(std::is_move_constructible_v<Image>);
        static_assert(std::is_nothrow_move_constructible_v<Image>);

        Image moved;
        Image copy = moved;
        const std::filesystem::path path = outputDir / "from_copy.png";
        copy.savePNG(rgb.data(), kWidth, kHeight, path);
        CHECK_MESSAGE(fileSize(path) > 0, "a copied Image must still encode");
    }

    std::cout << '\n' << (g_failures == 0 ? "PASSED" : "FAILED") << ": " << (g_checks - g_failures)
              << " of " << g_checks << " checks succeeded\n";

    std::filesystem::remove_all(outputDir, error);
    return g_failures == 0 ? 0 : 1;
}
