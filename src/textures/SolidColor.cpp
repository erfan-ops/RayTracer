#include <Texture.hpp>

SolidColor::SolidColor(const Color& albedo) : m_albedo(albedo) {}

SolidColor::SolidColor(double red, double green, double blue) : SolidColor(Color(red,green,blue)) {}

Color SolidColor::value(double u, double v, const Point3& p) const {
    return m_albedo;
}