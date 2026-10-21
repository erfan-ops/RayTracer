#include <Hittable.hpp>


void HitRecord::set_face_normal(const Ray& r, const Vec3& outward_normal) {
    front_face = dot(r.direction(), outward_normal) < 0.0;
    normal = front_face ? outward_normal : -outward_normal;
}


Translate::Translate(
    std::shared_ptr<Hittable> object,
    const Vec3& offset
) : object(object)
  , offset(offset)
{
    bbox = object->bounding_box() + offset;
}

bool Translate::hit(const Ray& r, Interval ray_t, HitRecord& rec) const {
    // Move the ray backwards by the offset
    Ray offset_r(r.origin() - offset, r.direction(), r.time());

    if (!object->hit(offset_r, ray_t, rec))
        return false;

    rec.p += offset;

    return true;
}

AABB Translate::bounding_box() const { return bbox; }




RotateY::RotateY(
    std::shared_ptr<Hittable> object,
    double angle
) : object(object)
{
    auto radians = degrees_to_radians(angle);
    sin_theta = std::sin(radians);
    cos_theta = std::cos(radians);
    bbox = object->bounding_box();

    Point3 min( infinity,  infinity,  infinity);
    Point3 max(-infinity, -infinity, -infinity);

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                auto x = i*bbox.x.max() + (1-i)*bbox.x.min();
                auto y = j*bbox.y.max() + (1-j)*bbox.y.min();
                auto z = k*bbox.z.max() + (1-k)*bbox.z.min();

                auto newx =  cos_theta*x + sin_theta*z;
                auto newz = -sin_theta*x + cos_theta*z;

                Vec3 tester(newx, y, newz);

                for (int c = 0; c < 3; c++) {
                    min[c] = std::fmin(min[c], tester[c]);
                    max[c] = std::fmax(max[c], tester[c]);
                }
            }
        }
    }

    bbox = AABB(min, max);
}

bool RotateY::hit(const Ray& r, Interval ray_t, HitRecord& rec) const {

    // Transform the ray from world space to object space.

    auto origin = Point3(
        (cos_theta * r.origin().x()) - (sin_theta * r.origin().z()),
        r.origin().y(),
        (sin_theta * r.origin().x()) + (cos_theta * r.origin().z())
    );

    auto direction = Vec3(
        (cos_theta * r.direction().x()) - (sin_theta * r.direction().z()),
        r.direction().y(),
        (sin_theta * r.direction().x()) + (cos_theta * r.direction().z())
    );

    Ray rotated_r(origin, direction, r.time());

    // Determine whether an intersection exists in object space (and if so, where).

    if (!object->hit(rotated_r, ray_t, rec))
        return false;

    // Transform the intersection from object space back to world space.

    rec.p = Point3(
        (cos_theta * rec.p.x()) + (sin_theta * rec.p.z()),
        rec.p.y(),
        (-sin_theta * rec.p.x()) + (cos_theta * rec.p.z())
    );

    rec.normal = Vec3(
        (cos_theta * rec.normal.x()) + (sin_theta * rec.normal.z()),
        rec.normal.y(),
        (-sin_theta * rec.normal.x()) + (cos_theta * rec.normal.z())
    );

    return true;
}

AABB RotateY::bounding_box() const { return bbox; }
