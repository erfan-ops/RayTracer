#pragma once

#include <Vec3.hpp>
#include <Hittable.hpp>

class Material {
public:
    virtual ~Material() = default;

    virtual bool scatter(
        const Ray& r_in,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const;
};


class Lambertian : public Material {
public:
    Lambertian(const Color& albedo);

    bool scatter(
        const Ray& r_in,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const override;

private:
    Color m_albedo;
};

class Metal : public Material {
public:
    Metal(const Color& albedo, double fuzz);

    bool scatter(
        const Ray& r_in,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const override;

  private:
    Color m_albedo;
    double m_fuzz;
};