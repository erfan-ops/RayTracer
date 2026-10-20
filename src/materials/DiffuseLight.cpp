#include <Material.hpp>

DiffuseLight::DiffuseLight(std::shared_ptr<Texture> tex) : tex(tex) {}
DiffuseLight::DiffuseLight(const Color& emit) : tex(std::make_shared<SolidColor>(emit)) {}

Color DiffuseLight::emitted(double u, double v, const Point3& p) const {
    return tex->value(u, v, p);
}