#pragma once

#include <Hittable.hpp>
#include <Vec3.hpp>
#include <Interval.hpp>
#include <Material.hpp>

class Sphere : public Hittable {
public:
    Sphere(
        const Point3& static_center,
        double radius,
        std::shared_ptr<Material> mat
    );

    Sphere(
        const Point3& center1,
        const Point3& center2,
        double radius,
        std::shared_ptr<Material> mat
    );

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;

    AABB bounding_box() const override;

private:
    static void get_sphere_uv(const Point3& p, double& u, double& v);

private:
    Ray m_center;
    double m_radius;
    std::shared_ptr<Material> m_mat;
    AABB m_bbox;
};
