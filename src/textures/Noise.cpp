#include <Texture.hpp>
#include <Perlin.hpp>

NoiseTexture::NoiseTexture(
    double scale
) : m_scale(scale) {}

Color NoiseTexture::value(double u, double v, const Point3& p) const {
    return Color(0.5, 0.5, 0.5) * (1.0 + std::sin(m_scale * p.z() + 10.0 * noise.turb(p, 7)));
}