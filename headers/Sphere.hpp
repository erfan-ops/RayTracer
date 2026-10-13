#pragma once

#include <Hittable.hpp>
#include <Vec3.hpp>
#include <Interval.hpp>

class Sphere : public Hittable {
public:
    Sphere(const Point3& center, double radius);

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;

private:
    Point3 m_center;
    double m_radius;
};
