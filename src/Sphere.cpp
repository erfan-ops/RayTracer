#include <Sphere.hpp>

Sphere::Sphere(const Point3& center, float radius) : m_center(center), m_radius(std::fmax(0.0f,radius)) {}

bool Sphere::hit(const Ray& r, Interval ray_t, HitRecord& rec) const {
    Vec3 oc = m_center - r.origin();
    float a = r.direction().length_squared();
    float h = dot(r.direction(), oc);
    float c = oc.length_squared() - m_radius*m_radius;

    float discriminant = h*h - a*c;
    if (discriminant < 0.0f)
        return false;

    float sqrtd = std::sqrt(discriminant);

    // Find the nearest root that lies in the acceptable range.
    float root = (h - sqrtd) / a;
    if (!ray_t.surrounds(root)) {
        root = (h + sqrtd) / a;
        if (!ray_t.surrounds(root))
            return false;
    }

    rec.t = root;
    rec.p = r.at(rec.t);

    Vec3 outward_normal = (rec.p - m_center) / m_radius;
    rec.set_face_normal(r, outward_normal);

    return true;
}

