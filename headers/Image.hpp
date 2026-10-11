#pragma once

#include <filesystem>

/**
 * @file Image.hpp
 * @brief Writes raw pixel buffers to image files using FFmpeg.
 *
 * The class is a thin, allocation-free façade over libavcodec/libavformat
 * encoders. It holds no FFmpeg state of its own: every save operation
 * allocates the codec, frame, packet and muxer it needs locally and releases
 * them before returning, on success and on failure alike. No FFmpeg header is
 * exposed here, so callers do not need FFmpeg on their include path.
 */
class Image {
public:
    /**
     * @brief Layout of the pixel buffer handed to the save methods.
     *
     * Channels are interleaved and each channel is 8 bits wide. Rows are
     * assumed to be tightly packed (stride == width * bytesPerPixel) unless a
     * larger stride is supplied explicitly.
     */
    enum class PixelFormat {
        RGB24,  ///< 3 bytes per pixel: red, green, blue.
        RGBA32, ///< 4 bytes per pixel: red, green, blue, alpha.
        GRAY8,  ///< 1 byte per pixel: luminance.
    };

    /// Output codec/container combination understood by the save methods.
    enum class Format {
        PNG,  ///< Lossless, alpha-capable.
        JPEG, ///< Lossy, no alpha.
        BMP,  ///< Lossless, alpha-capable.
        WebP, ///< Lossy or lossless, alpha-capable in the lossless mode.
    };

    /**
     * @brief Constructs a stateless encoder.
     *
     * FFmpeg (verified against n9.0.2 / libavcodec 63) needs no process-wide
     * initialisation for encoding to a local file: av_register_all() and
     * avcodec_register_all() no longer exist, codecs are registered
     * automatically when the library loads, and avformat_network_init() is
     * only required for network protocols, which this class never uses.
     * Per-encoder set-up happens inside avcodec_open2() for each save
     * operation, so there is nothing left for this constructor to do.
     */
    Image() noexcept = default;
    ~Image() = default;

    // An Image owns no resources, so copying and moving are unremarkable. The
    // operations are spelled out rather than left implicit so that adding
    // state later cannot silently introduce an accidental FFmpeg resource copy.
    Image(const Image&) = default;
    Image(Image&&) noexcept = default;
    Image& operator=(const Image&) = default;
    Image& operator=(Image&&) noexcept = default;

    /**
     * @brief Reports whether the given format can be written by this FFmpeg build.
     *
     * Useful for probing capabilities before committing to an output format.
     * Never throws.
     */
    [[nodiscard]] static bool isFormatAvailable(Format format) noexcept;

    /**
     * @brief Encodes @p data as a PNG file.
     *
     * @param data             Pixel buffer, laid out as @p format describes.
     * @param width            Image width in pixels; must be positive.
     * @param height           Image height in pixels; must be positive.
     * @param outputPath       Destination file. Any existing file is replaced.
     * @param format           Layout of @p data; defaults to tightly packed RGB24.
     * @param rowStrideBytes   Distance between the starts of consecutive rows.
     *                         Zero means tightly packed (width * bytesPerPixel).
     *                         A non-zero value must be at least that large;
     *                         negative strides are not supported.
     * @param compressionLevel Deflate effort, 0 (store) to 9 (maximum), or -1
     *                         for the encoder default.
     *
     * @throws std::invalid_argument on a null buffer, non-positive dimensions,
     *         an out-of-range stride or compression level.
     * @throws std::runtime_error if the encoder is unavailable, encoding fails,
     *         or the file cannot be written. A failed call removes its partial
     *         output file rather than leaving it behind.
     */
    void savePNG(const unsigned char* data, const int width, const int height,
                 const std::filesystem::path& outputPath,
                 PixelFormat format = PixelFormat::RGB24,
                 int rowStrideBytes = 0,
                 int compressionLevel = -1) const;

    /**
     * @brief Encodes @p data as a JPEG file.
     *
     * JPEG has no alpha channel and is encoded as 4:2:0 chroma-subsampled
     * YCbCr, so an RGBA input loses its alpha and fine colour detail is
     * averaged over 2x2 blocks. Use savePNG() when pixel-exact output matters.
     *
     * @param quality Quantiser quality from 1 (smallest) to 100 (best).
     *
     * Other parameters, and the exceptions thrown, match savePNG().
     */
    void saveJPEG(const unsigned char* data, const int width, const int height,
                  const std::filesystem::path& outputPath,
                  PixelFormat format = PixelFormat::RGB24,
                  int rowStrideBytes = 0,
                  int quality = 90) const;

    /**
     * @brief Encodes @p data as a BMP file.
     *
     * Lossless and pixel-exact. Parameters, and the exceptions thrown, match
     * savePNG().
     */
    void saveBMP(const unsigned char* data, const int width, const int height,
                 const std::filesystem::path& outputPath,
                 PixelFormat format = PixelFormat::RGB24,
                 int rowStrideBytes = 0) const;

    /**
     * @brief Encodes @p data as a WebP file.
     *
     * @param quality  Encoding quality from 1 (smallest) to 100 (best).
     *                 Ignored when @p lossless is set.
     * @param lossless When set, produces a pixel-exact file; @p quality is
     *                 then ignored. Alpha is preserved in this mode.
     *
     * Other parameters, and the exceptions thrown, match saveJPEG().
     */
    void saveWebP(const unsigned char* data, const int width, const int height,
                  const std::filesystem::path& outputPath,
                  PixelFormat format = PixelFormat::RGB24,
                  int rowStrideBytes = 0,
                  int quality = 90,
                  bool lossless = false) const;
};
