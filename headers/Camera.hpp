#pragma once

#include <Vec3.hpp>
#include <Renderer.hpp>
#include <HittableList.hpp>

class Camera {
public:
    Camera(
        const double verticalFOV,
        const Point3 &lookfrom,
        const Point3 &lookat,
        const Vec3 &vup,
        const size_t imageWidth,
        const size_t imageHeight,
        const int samplesPerPixel,
        const int maxDepth,
        const double defocus_angle,
        const double focus_dist
    );
    ~Camera();

    const unsigned char* shot(HittableList world) const;

    const size_t width() const;
    const size_t height() const;

private:
    Ray get_ray(int i, int j) const;

    Color ray_color(const Ray& r, const int depth, const Hittable& world) const;
    Point3 defocus_disk_sample() const;

private:
    size_t m_imageWidth;
    size_t m_imageHeight;

    double m_viewportWidth;
    double m_viewportHeight;

    double m_focalLength{1.0};
    Point3 m_center;

    Vec3 m_viewportU;
    Vec3 m_viewportV;

    Vec3 m_pixelDeltaU;
    Vec3 m_pixelDeltaV;

    Vec3 m_viewportUpperLeft;
    Vec3 m_pixel00Loc;

    Renderer* m_renderer{nullptr};

    int m_samplesPerPixel;
    double m_pixelSamplesScale;
    int m_maxDepth;

    double m_vfov = 90;  // Vertical view angle (field of view)

    Vec3 u, v, w;

    Vec3 m_defocusDiskU;
    Vec3 m_defocusDiskV;

    double m_defocusAngle;
    double m_focusDist;
};
