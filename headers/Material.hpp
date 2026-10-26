#pragma once

#include <Vec3.hpp>
#include <Hittable.hpp>
#include <Texture.hpp>


class Material {
public:
    virtual ~Material() = default;

    virtual bool scatter(
        const Ray& r_in,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const;

    virtual Color emitted(double u, double v, const Point3& p) const;
    virtual double scattering_pdf(const Ray& r_in, const HitRecord& rec, const Ray& scattered) const;
};


class Lambertian : public Material {
public:
    Lambertian(const Color& albedo);
    Lambertian(std::shared_ptr<Texture> tex);

    bool scatter(
        const Ray& r_in,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const override;

    double scattering_pdf(const Ray& r_in, const HitRecord& rec, const Ray& scattered) const override;

private:
    std::shared_ptr<Texture> m_tex;
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


class Dielectric : public Material {
public:
    Dielectric(double refraction_index);

    bool scatter(
        const Ray& r_in,
        const HitRecord& rec,
        Color& attenuation,
        Ray& scattered
    ) const override;

private:
    static double reflectance(double cosine, double refraction_index) {
        // Use Schlick's approximation for reflectance.
        double r0 = (1.0 - refraction_index) / (1.0 + refraction_index);
        r0 = r0*r0;
        return r0 + (1.0-r0)*std::pow((1.0 - cosine),5.0);
    }

private:
    double m_refractionIndex;
};


class DiffuseLight : public Material {
public:
    DiffuseLight(std::shared_ptr<Texture> tex);
    DiffuseLight(const Color& emit);

    Color emitted(double u, double v, const Point3& p) const override;

private:
    std::shared_ptr<Texture> tex;
};


class Isotropic : public Material {
public:
    Isotropic(const Color& albedo);
    Isotropic(std::shared_ptr<Texture> tex);

    bool scatter(const Ray& r_in, const HitRecord& rec, Color& attenuation, Ray& scattered) const override;

private:
    std::shared_ptr<Texture> tex;
};
