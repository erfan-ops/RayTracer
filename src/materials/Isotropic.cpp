#include <Material.hpp>


Isotropic::Isotropic(
    const Color& albedo
) : tex(std::make_shared<SolidColor>(albedo))
{}

Isotropic::Isotropic(
    std::shared_ptr<Texture> tex
) : tex(tex)
{}

bool Isotropic::scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const {
    scattered = Ray(rec.p, random_unit_vector(), r_in.time());
    attenuation = tex->value(rec.u, rec.v, rec.p);
    return true;
}

