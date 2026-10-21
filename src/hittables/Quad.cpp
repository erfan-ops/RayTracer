#include <hittables/Quad.hpp>


Quad::Quad(
    const Point3& Q,
    const Vec3& u,
    const Vec3& v,
    std::shared_ptr<Material> mat
) : Q(Q), u(u), v(v), mat(mat)
{
    auto n = cross(u, v);
    normal = unit_vector(n);
    D = dot(normal, Q);
    w = n / dot(n,n);
    
    set_bounding_box();
}

void Quad::set_bounding_box() {
    auto bbox_diagonal1 = AABB(Q, Q + u + v);
    auto bbox_diagonal2 = AABB(Q + u, Q + v);
    bbox = AABB(bbox_diagonal1, bbox_diagonal2);
}

AABB Quad::bounding_box() const { return bbox; }

bool Quad::hit(const Ray& r, Interval ray_t, HitRecord& rec) const {
    auto denom = dot(normal, r.direction());

    // No hit if the ray is parallel to the plane.
    if (std::fabs(denom) < 1e-8)
        return false;

    // Return false if the hit point parameter t is outside the ray interval.
    auto t = (D - dot(normal, r.origin())) / denom;
    if (!ray_t.contains(t))
        return false;

    auto intersection = r.at(t);
    Vec3 planar_hitpt_vector = intersection - Q;
    auto alpha = dot(w, cross(planar_hitpt_vector, v));
    auto beta = dot(w, cross(u, planar_hitpt_vector));

    if (!is_interior(alpha, beta, rec))
        return false;
    
    // ADDITIONAL FUN :D
    // double a, b;
    // a = alpha * 2.0 - 1.0;
    // b = beta * 2.0 - 1.0;
    // if (a*a + b*b > 1.0 || a*a + b*b < 0.8)
    //     return false;

    // Ray hits the 2D shape; set the rest of the hit record and return true.

    rec.t = t;
    rec.p = intersection;
    rec.mat = mat;
    rec.set_face_normal(r, normal);

    return true;
}

bool Quad::is_interior(double a, double b, HitRecord& rec) const {
    Interval unit_interval = Interval(0, 1);

    if (!unit_interval.contains(a) || !unit_interval.contains(b))
        return false;

    rec.u = a;
    rec.v = b;
    return true;
}
