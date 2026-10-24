#include <hittables/Triangle.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

Triangle::Triangle(
    const Point3& a,
    const Point3& b,
    const Point3& c,
    std::shared_ptr<Material> mat
) : a(a)
  , b(b)
  , c(c)
  , edge1(b - a)
  , edge2(c - a)
  , mat(std::move(mat))
{
    // Compute the unnormalized normal.
    const Vec3 n = cross(edge1, edge2);
    const double n_squared = dot(n, n);

    // Reject degenerate or numerically unstable triangles.
    if (n_squared < 1e-20)
        throw std::invalid_argument(
            "Triangle vertices must not be collinear or coincident."
        );

    normal = n / std::sqrt(n_squared);

    // Plane equation: dot(normal, P) = D.
    D = dot(normal, a);

    // Used to calculate barycentric coordinates.
    w = n / n_squared;

    set_bounding_box();
}

void Triangle::set_bounding_box() {
    // Component-wise minimum and maximum of all three vertices.
    const Point3 min_point(
        std::min({a.x(), b.x(), c.x()}),
        std::min({a.y(), b.y(), c.y()}),
        std::min({a.z(), b.z(), c.z()})
    );

    const Point3 max_point(
        std::max({a.x(), b.x(), c.x()}),
        std::max({a.y(), b.y(), c.y()}),
        std::max({a.z(), b.z(), c.z()})
    );

    // Ensure the bounding box has nonzero thickness on every axis.
    constexpr double padding = 1e-4;

    const Point3 padded_min(
        min_point.x() - padding,
        min_point.y() - padding,
        min_point.z() - padding
    );

    const Point3 padded_max(
        max_point.x() + padding,
        max_point.y() + padding,
        max_point.z() + padding
    );

    bbox = AABB(padded_min, padded_max);
}

AABB Triangle::bounding_box() const {
    return bbox;
}

bool Triangle::hit(
    const Ray& r,
    Interval ray_t,
    HitRecord& rec
) const {
    // Intersect the ray with the triangle's supporting plane.
    const double denom = dot(normal, r.direction());

    // The ray is parallel or nearly parallel to the plane.
    if (std::fabs(denom) < 1e-8)
        return false;

    const double t =
        (D - dot(normal, r.origin())) / denom;

    if (!ray_t.contains(t))
        return false;

    const Point3 intersection = r.at(t);
    const Vec3 p = intersection - a;

    // Calculate barycentric coordinates.
    const double alpha = dot(w, cross(p, edge2));
    const double beta  = dot(w, cross(edge1, p));

    // The point must lie inside the triangle.
    if (alpha < 0.0 ||
        beta < 0.0 ||
        alpha + beta > 1.0)
    {
        return false;
    }

    // Populate the hit record.
    rec.t = t;
    rec.p = intersection;
    rec.u = alpha;
    rec.v = beta;
    rec.mat = mat;
    rec.set_face_normal(r, normal);

    return true;
}