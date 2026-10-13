#include <Sphere.hpp>

Sphere::Sphere(
    const Point3& center,
    double radius,
    std::shared_ptr<Material> mat
) : m_center(center)
  , m_radius(std::fmax(0.0,radius))
  , m_mat(mat) {
    // TODO: Initialize the material pointer `mat`.
}

bool Sphere::hit(const Ray& r, Interval ray_t, HitRecord& rec) const {
    Vec3 oc = m_center - r.origin();
    double a = r.direction().length_squared();
    double h = dot(r.direction(), oc);
    double c = oc.length_squared() - m_radius*m_radius;

    double discriminant = h*h - a*c;
    if (discriminant < 0.0)
        return false;

    double sqrtd = std::sqrt(discriminant);

    // Find the nearest root that lies in the acceptable range.
    double root = (h - sqrtd) / a;
    if (!ray_t.surrounds(root)) {
        root = (h + sqrtd) / a;
        if (!ray_t.surrounds(root))
            return false;
    }

    rec.t = root;
    rec.p = r.at(rec.t);

    Vec3 outward_normal = (rec.p - m_center) / m_radius;
    rec.set_face_normal(r, outward_normal);
    rec.mat = m_mat;

    return true;
}

