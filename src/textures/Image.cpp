#include <Texture.hpp>
#include <Interval.hpp>

ImageTexture::ImageTexture(
    const std::string& imagePath
) : img(imagePath)
{}

Color ImageTexture::value(double u, double v, const Point3& p) const {
    // The constructor loads the image, so the reader is always populated here.
    // ImageRead already returns linear-light samples, which is the space the
    // renderer shades in.

    // Clamp input texture coordinates to [0,1] x [1,0]
    u = Interval(0,1).clamp(u);
    v = 1.0 - Interval(0,1).clamp(v);  // Flip V to image coordinates

    // A coordinate of exactly 1.0 maps to width/height, one past the last
    // pixel, so clamp the resulting indices rather than the coordinates.
    const int i = std::min(int(u * img.width()), img.width() - 1);
    const int j = std::min(int(v * img.height()), img.height() - 1);
    auto pixel = img.pixel_data(i,j);

    return pixel;
}