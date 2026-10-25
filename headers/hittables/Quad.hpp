#pragma once

#include <Hittable.hpp>
#include <HittableList.hpp>

class Quad : public Hittable {
public:
    Quad(const Point3& Q, const Vec3& u, const Vec3& v, std::shared_ptr<Material> mat, double cornerRadius = 0);

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
    double radius;
};


inline std::shared_ptr<HittableList> box(const Point3& a, const Point3& b, std::shared_ptr<Material> mat)
{
    // Returns the 3D box (six sides) that contains the two opposite vertices a & b.

    auto sides = std::make_shared<HittableList>();

    // Construct the two opposite vertices with the minimum and maximum coordinates.
    auto min = Point3(std::fmin(a.x(),b.x()), std::fmin(a.y(),b.y()), std::fmin(a.z(),b.z()));
    auto max = Point3(std::fmax(a.x(),b.x()), std::fmax(a.y(),b.y()), std::fmax(a.z(),b.z()));

    auto dx = Vec3(max.x() - min.x(), 0, 0);
    auto dy = Vec3(0, max.y() - min.y(), 0);
    auto dz = Vec3(0, 0, max.z() - min.z());

    sides->add(std::make_shared<Quad>(Point3(min.x(), min.y(), max.z()),  dx,  dy, mat)); // front
    sides->add(std::make_shared<Quad>(Point3(max.x(), min.y(), max.z()), -dz,  dy, mat)); // right
    sides->add(std::make_shared<Quad>(Point3(max.x(), min.y(), min.z()), -dx,  dy, mat)); // back
    sides->add(std::make_shared<Quad>(Point3(min.x(), min.y(), min.z()),  dz,  dy, mat)); // left
    sides->add(std::make_shared<Quad>(Point3(min.x(), max.y(), max.z()),  dx, -dz, mat)); // top
    sides->add(std::make_shared<Quad>(Point3(min.x(), min.y(), min.z()),  dx,  dz, mat)); // bottom

    return sides;
}
