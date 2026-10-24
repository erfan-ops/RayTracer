#pragma once

#include <Hittable.hpp>

#include <memory>

class Triangle : public Hittable {
public:
    Triangle(
        const Point3& a,
        const Point3& b,
        const Point3& c,
        std::shared_ptr<Material> mat
    );

    AABB bounding_box() const override;

    bool hit(
        const Ray& r,
        Interval ray_t,
        HitRecord& rec
    ) const override;

private:
    void set_bounding_box();

    Point3 a, b, c;
    Vec3 edge1, edge2;
    Vec3 normal;
    Vec3 w;
    double D;

    std::shared_ptr<Material> mat;
    AABB bbox;
};