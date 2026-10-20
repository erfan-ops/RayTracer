#include <Material.hpp>


bool Material::scatter(
    const Ray& r_in,
    const HitRecord& rec,
    Color& attenuation,
    Ray& scattered
) const {
    return false;
}

Color Material::emitted(double u, double v, const Point3& p) const {
    return Color(0.0, 0.0, 0.0);
}
