#pragma once

#include <Hittable.hpp>
#include <Material.hpp>
#include <Texture.hpp>

class ConstantMedium : public Hittable {
public:
    ConstantMedium(std::shared_ptr<Hittable> boundary, double density, std::shared_ptr<Texture> tex);

    ConstantMedium(std::shared_ptr<Hittable> boundary, double density, const Color& albedo);

    bool hit(const Ray& r, Interval ray_t, HitRecord& rec) const override;

    AABB bounding_box() const override;

private:
    std::shared_ptr<Hittable> boundary;
    double neg_inv_density;
    std::shared_ptr<Material> phase_function;
};
