#include <Texture.hpp>

GradientTexture::GradientTexture(
    const Color& color1,
    const Color& color2,
    Vec3 direction,
    double length
) : m_color1(color1)
  , m_color2(color2)
  , m_direction(direction)
  , m_length(length)
{
    m_direction.normalize();
}

Color GradientTexture::value(double u, double v, const Point3& p) const {
    const double d = dot(p, m_direction);
    
    double t = std::fmod(std::abs(d), m_length) / m_length;

    return (1.0 - t) * m_color1 + t * m_color2;
}