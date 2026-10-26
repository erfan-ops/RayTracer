#include <Material.hpp>

Lambertian::Lambertian(const Color& albedo) : m_tex(std::make_shared<SolidColor>(albedo)) {}
Lambertian::Lambertian(std::shared_ptr<Texture> tex) : m_tex(tex) {}

bool Lambertian::scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const {
    auto scatter_direction = rec.normal + random_unit_vector();
    scattered = Ray(rec.p, scatter_direction, r_in.time());
    attenuation = m_tex->value(rec.u, rec.v, rec.p);
    return true;
}

double Lambertian::scattering_pdf(const Ray& r_in, const HitRecord& rec, const Ray& scattered) const {
    auto cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
    return cos_theta < 0 ? 0 : cos_theta/std::numbers::pi;
}