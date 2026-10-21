#pragma once

#include <Ray.hpp>
#include <Interval.hpp>
#include <AABB.hpp>

class Material;

class HitRecord {
public:
    Point3 p;
    Vec3 normal;
    std::shared_ptr<Material> mat;
    double t;

    double u;
    double v;

    bool front_face;

    void set_face_normal(const Ray& r, const Vec3& outward_normal);
};

class Hittable {
public:
    virtual ~Hittable() = default;

    virtual bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const = 0;

    virtual AABB bounding_box() const = 0;
};

class Translate : public Hittable {
public:
    Translate(std::shared_ptr<Hittable> object, const Vec3& offset);

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;

    AABB bounding_box() const override;

private:
    std::shared_ptr<Hittable> object;
    Vec3 offset;
    AABB bbox;
};

class RotateY : public Hittable {
public:
    RotateY(std::shared_ptr<Hittable> object, double angle);

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;

    AABB bounding_box() const override;

private:
    std::shared_ptr<Hittable> object;
    double sin_theta;
    double cos_theta;
    AABB bbox;
};
