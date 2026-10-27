#include <Material.hpp>

Lambertian::Lambertian(const Color& albedo) : m_tex(std::make_shared<SolidColor>(albedo)) {}
Lambertian::Lambertian(std::shared_ptr<Texture> tex) : m_tex(tex) {}

bool Lambertian::scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const {
    auto scatter_direction = random_on_hemisphere(rec.normal);
    scattered = Ray(rec.p, scatter_direction, r_in.time());
    attenuation = m_tex->value(rec.u, rec.v, rec.p);
    return true;
}

double Lambertian::scattering_pdf(const Ray& r_in, const HitRecord& rec, const Ray& scattered) const {
    return 1 / (2*std::numbers::pi);
}