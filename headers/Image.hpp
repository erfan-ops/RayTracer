#pragma once

#include <filesystem>
#include <vector>

#include <Vec3.hpp>

/**
 * @file Image.hpp
 * @brief Reads and writes image files using FFmpeg.
 *
 * ImageWrite is a thin, allocation-free façade over libavcodec/libavformat
 * encoders. It holds no FFmpeg state of its own: every save operation
 * allocates the codec, frame, packet and muxer it needs locally and releases
 * them before returning, on success and on failure alike.
 *
 * ImageRead decodes a file once into an in-memory grid of linear-light RGB and
 * serves pixels from that buffer. It owns its storage, so a loaded image
 * outlives the FFmpeg objects used to produce it.
 *
 * No FFmpeg header is exposed here, so callers do not need FFmpeg on their
 * include path.
 */
class ImageWrite {
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
    ImageWrite() noexcept = default;
    ~ImageWrite() = default;

    // An Image owns no resources, so copying and moving are unremarkable. The
    // operations are spelled out rather than left implicit so that adding
    // state later cannot silently introduce an accidental FFmpeg resource copy.
    ImageWrite(const ImageWrite&) = default;
    ImageWrite(ImageWrite&&) noexcept = default;
    ImageWrite& operator=(const ImageWrite&) = default;
    ImageWrite& operator=(ImageWrite&&) noexcept = default;

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

/**
 * @brief Decodes an image file into an in-memory RGB grid using FFmpeg.
 *
 * load() opens the file with libavformat, decodes the first frame of the
 * selected stream with libavcodec, converts it once to tightly packed 8-bit
 * RGB24 with libswscale and keeps that buffer. pixel_data() reads from the
 * buffer directly and never decodes again. For animated or multi-frame inputs
 * only the first decoded frame is retained, which is what a still-image caller
 * expects.
 *
 * Decoded samples are assumed to be sRGB-encoded, the conventional encoding
 * for PNG and JPEG. They are converted through the sRGB transfer function so
 * that Color receives linear-light values in [0.0, 1.0], matching the space
 * the renderer shades in; formats that already store linear light are not
 * detected. Color has no alpha channel, so alpha in the source file is
 * discarded rather than composited: an RGBA source contributes only its red,
 * green and blue samples.
 *
 * The class owns its pixel buffer. Copies duplicate it. A moved-from object is
 * left unloaded, and its pixel_data() throws std::logic_error like any other
 * empty reader.
 */
class ImageRead {
public:
    /// Constructs an unloaded reader. width(), height() and pixel_data() throw
    /// std::logic_error until an image has been loaded successfully.
    ImageRead();

    /**
     * @brief Constructs a reader and loads @p path.
     *
     * Equivalent to default construction followed by load(path). If loading
     * fails, the exception propagates and no object is left half-built.
     *
     * @throws std::invalid_argument if @p path is empty.
     * @throws std::runtime_error if the file cannot be opened or decoded, or
     *         its content is not an image this FFmpeg build can decode.
     * @throws std::bad_alloc if memory for the decoded pixels cannot be
     *         allocated.
     */
    explicit ImageRead(const std::filesystem::path& path);

    ~ImageRead();

    // The pixel buffer is owned through std::vector, so copies get their own
    // storage and moves cannot leave two objects referring to one buffer.
    // Moves are spelled out to reset the source to the well-defined unloaded
    // state rather than leaving a moved-from reader with stale dimensions.
    ImageRead(const ImageRead& other);
    ImageRead(ImageRead&& other) noexcept;
    ImageRead& operator=(const ImageRead& other);
    ImageRead& operator=(ImageRead&& other) noexcept;

    /**
     * @brief Loads and decodes @p path, replacing the current image.
     *
     * Provides the strong exception guarantee: if loading fails, the
     * previously loaded image, if any, is left unchanged.
     *
     * The selected stream is the first video stream marked as an attached
     * picture (a still image carried by a container) or, failing that, the
     * best video stream FFmpeg finds. Only the first frame it decodes is
     * retained; animated formats are not played.
     *
     * @throws std::invalid_argument if @p path is empty.
     * @throws std::runtime_error if the file cannot be opened or decoded, or
     *         its content is not an image this FFmpeg build can decode.
     * @throws std::bad_alloc if memory for the decoded pixels cannot be
     *         allocated.
     */
    void load(const std::filesystem::path& path);

    /**
     * @brief Returns the width of the loaded image in pixels.
     * @throws std::logic_error if no image is loaded.
     */
    [[nodiscard]] int width() const;

    /**
     * @brief Returns the height of the loaded image in pixels.
     * @throws std::logic_error if no image is loaded.
     */
    [[nodiscard]] int height() const;

    /**
     * @brief Returns the colour of the pixel at (@p x, @p y).
     *
     * (0, 0) is the top-left pixel, x grows to the right and y downwards.
     * Each channel is the decoded 8-bit sRGB sample converted to linear light
     * in [0.0, 1.0].
     *
     * @throws std::logic_error if no image is loaded.
     * @throws std::out_of_range if @p x or @p y is negative or outside the
     *         loaded dimensions.
     */
    [[nodiscard]] const Color pixel_data(int x, int y) const;

private:
    /// Throws std::logic_error unless a complete image is present.
    void requireLoaded() const;

    int width_ = 0;
    int height_ = 0;
    /// Tightly packed RGB24: width_ * height_ * 3 bytes, row-major from the
    /// top-left pixel.
    std::vector<unsigned char> pixels_;
};
