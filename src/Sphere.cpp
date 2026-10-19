#include <Sphere.hpp>

Sphere::Sphere(
    const Point3& static_center,
    double radius,
    std::shared_ptr<Material> mat
) : m_center(static_center, Vec3(0.0, 0.0, 0.0))
  , m_radius(std::fmax(0.0, radius))
  , m_mat(mat)
{
    Vec3 rvec = Vec3(radius, radius, radius);
    m_bbox = AABB(static_center - rvec, static_center + rvec);
}

Sphere::Sphere(
    const Point3& center1,
    const Point3& center2,
    double radius,
    std::shared_ptr<Material> mat
) : m_center(center1, center2 - center1)
  , m_radius(std::fmax(0.0, radius))
  , m_mat(mat)
{
    Vec3 rvec = Vec3(radius, radius, radius);
    AABB box1(m_center.at(0) - rvec, m_center.at(0) + rvec);
    AABB box2(m_center.at(1) - rvec, m_center.at(1) + rvec);
    m_bbox = AABB(box1, box2);
}

bool Sphere::hit(const Ray& r, Interval ray_t, HitRecord& rec) const {
    Point3 current_center = m_center.at(r.time());
    Vec3 oc = current_center - r.origin();
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

    Vec3 outward_normal = (rec.p - current_center) / m_radius;
    rec.set_face_normal(r, outward_normal);
    rec.mat = m_mat;

    return true;
}

AABB Sphere::bounding_box() const { return m_bbox; }

