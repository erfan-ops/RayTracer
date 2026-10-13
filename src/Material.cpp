#include <Material.hpp>



bool Material::scatter(
    const Ray& r_in,
    const HitRecord& rec,
    Color& attenuation,
    Ray& scattered
) const {
    return false;
}



Lambertian::Lambertian(const Color& albedo) : m_albedo(albedo) {}

bool Lambertian::scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const {
    auto scatter_direction = rec.normal + random_unit_vector();
    scattered = Ray(rec.p, scatter_direction);
    attenuation = m_albedo;
    return true;
}



Metal::Metal(
    const Color& albedo,
    double fuzz
) : m_albedo(albedo)
  , m_fuzz(fuzz < 1 ? fuzz : 1) {}

bool Metal::scatter(
    const Ray& r_in,
    const HitRecord& rec,
    Color& attenuation,
    Ray& scattered
) const {
    Vec3 reflected = reflect(r_in.direction(), rec.normal);
    reflected = unit_vector(reflected) + (m_fuzz * random_unit_vector());
    scattered = Ray(rec.p, reflected);
    attenuation = m_albedo;
    return (dot(scattered.direction(), rec.normal) > 0);
}

