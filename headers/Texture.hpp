#pragma once

#include <Vec3.hpp>

#include <vector>

class Texture {
  public:
    virtual ~Texture() = default;

    virtual Color value(double u, double v, const Point3& p) const = 0;
};

class SolidColor : public Texture {
public:
    SolidColor(const Color& albedo);

    SolidColor(double red, double green, double blue);

    Color value(double u, double v, const Point3& p) const override;

private:
    Color m_albedo;
};

class CheckerTexture : public Texture {
public:
    CheckerTexture(double scale, std::shared_ptr<Texture> even, std::shared_ptr<Texture> odd);

    CheckerTexture(double scale, const Color& c1, const Color& c2);

    Color value(double u, double v, const Point3& p) const override;

private:
    double inv_scale;
    std::shared_ptr<Texture> even;
    std::shared_ptr<Texture> odd;
};

class GradientTexture : public Texture {
public:
    GradientTexture(const Color& color1, const Color& color2, Vec3 direction);

    Color value(double u, double v, const Point3& p) const override;

private:
    Color m_color1;
    Color m_color2;
    Vec3 m_direction;
};

class MultiGradientTexture : public Texture {
public:
    MultiGradientTexture(const std::vector<Color>& colors, Vec3 direction);

    Color value(double u, double v, const Point3& p) const override;

private:
    std::vector<Color> m_colors;
    Vec3 m_direction;
};
