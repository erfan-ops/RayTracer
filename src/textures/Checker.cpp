#include <Texture.hpp>

CheckerTexture::CheckerTexture(
    double scale,
    std::shared_ptr<Texture> even,
    std::shared_ptr<Texture> odd
) : inv_scale(1.0 / scale)
  , even(even)
  , odd(odd)
{}

CheckerTexture::CheckerTexture(
    double scale,
    const Color& c1,
    const Color& c2
) : CheckerTexture(scale, std::make_shared<SolidColor>(c1)
  , std::make_shared<SolidColor>(c2))
{}

Color CheckerTexture::value(double u, double v, const Point3& p) const {
    auto xInteger = int(std::floor(inv_scale * p.x()));
    auto yInteger = int(std::floor(inv_scale * p.y()));
    auto zInteger = int(std::floor(inv_scale * p.z()));

    bool isEven = (xInteger + yInteger + zInteger) % 2 == 0;

    return isEven ? even->value(u, v, p) : odd->value(u, v, p);
}