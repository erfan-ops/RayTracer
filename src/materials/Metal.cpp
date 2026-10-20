#include <Material.hpp>

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
    scattered = Ray(rec.p, reflected, r_in.time());
    attenuation = m_albedo;
    return (dot(scattered.direction(), rec.normal) > 0);
}