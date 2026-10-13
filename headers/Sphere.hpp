#pragma once

#include <Hittable.hpp>
#include <Vec3.hpp>
#include <Interval.hpp>
#include <Material.hpp>

class Sphere : public Hittable {
public:
    Sphere(
        const Point3& center,
        double radius,
        std::shared_ptr<Material> mat
    );

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;

private:
    Point3 m_center;
    double m_radius;
    std::shared_ptr<Material> m_mat;
};
