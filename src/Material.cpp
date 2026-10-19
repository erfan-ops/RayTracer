#include <Material.hpp>



bool Material::scatter(
    const Ray& r_in,
    const HitRecord& rec,
    Color& attenuation,
    Ray& scattered
) const {
    return false;
}



Lambertian::Lambertian(const Color& albedo) : m_tex(std::make_shared<SolidColor>(albedo)) {}
Lambertian::Lambertian(std::shared_ptr<Texture> tex) : m_tex(tex) {}

bool Lambertian::scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const {
    auto scatter_direction = rec.normal + random_unit_vector();
    scattered = Ray(rec.p, scatter_direction, r_in.time());
    attenuation = m_tex->value(rec.u, rec.v, rec.p);
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
    scattered = Ray(rec.p, reflected, r_in.time());
    attenuation = m_albedo;
    return (dot(scattered.direction(), rec.normal) > 0);
}



Dielectric::Dielectric(double refraction_index) : m_refractionIndex(refraction_index) {}

bool Dielectric::scatter(
    const Ray& r_in,
    const HitRecord& rec,
    Color& attenuation,
    Ray& scattered
) const {
    attenuation = Color(1.0, 1.0, 1.0);
    double ri = rec.front_face ? (1.0/m_refractionIndex) : m_refractionIndex;

    Vec3 unit_direction = unit_vector(r_in.direction());
    double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
    double sin_theta = std::sqrt(1.0 - cos_theta*cos_theta);

    bool cannot_refract = ri * sin_theta > 1.0;
    Vec3 direction;

    if (cannot_refract || reflectance(cos_theta, ri) > random_double())
        direction = reflect(unit_direction, rec.normal);
    else
        direction = refract(unit_direction, rec.normal, ri);

    scattered = Ray(rec.p, direction, r_in.time());
    return true;
}
