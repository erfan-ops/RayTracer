#include <Texture.hpp>
#include <Interval.hpp>

ImageTexture::ImageTexture(
    const std::string& imagePath,
    const std::string& ambientOcclusionPath
) : img(imagePath)
  , ao(ambientOcclusionPath)
{}

Color ImageTexture::value(double u, double v, const Point3& p) const {
    u = Interval(0,1).clamp(u);
    v = 1.0 - Interval(0,1).clamp(v);

    const int i = std::min(int(u * img.width()), img.width() - 1);
    const int j = std::min(int(v * img.height()), img.height() - 1);
    auto pixel = img.pixel_data(i,j);
    auto ambientOcclusion = ao.pixel_data(i, j);

    auto finalColor = pixel * ambientOcclusion;

    return finalColor;
}