#pragma once

#include "Hittable.hpp"

class Quad : public Hittable {
public:
    Quad(const Point3& Q, const Vec3& u, const Vec3& v, std::shared_ptr<Material> mat);

    virtual void set_bounding_box();

    virtual bool is_interior(double a, double b, HitRecord& rec) const;

    AABB bounding_box() const override;

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;

private:
    Point3 Q;
    Vec3 u, v;
    Vec3 w;
    std::shared_ptr<Material> mat;
    AABB bbox;
    Vec3 normal;
    double D;
};
