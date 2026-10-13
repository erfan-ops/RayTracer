#pragma once

#include <Vec3.hpp>
#include <Renderer.hpp>
#include <HittableList.hpp>

class Camera {
public:
    Camera(
        const float focalLength,
        const Point3 &center,
        const size_t imageWidth,
        const size_t imageHeight,
        const int samplesPerPixel = 10
    );
    ~Camera();

    const unsigned char* shot(HittableList world) const;

    const size_t width() const;
    const size_t height() const;

private:
    Ray get_ray(int i, int j) const;

private:
    size_t m_imageWidth;
    size_t m_imageHeight;

    float m_viewportWidth;
    float m_viewportHeight{2.0f};

    float m_focalLength;
    Point3 m_center;

    Vec3 m_viewportU;
    Vec3 m_viewportV;

    Vec3 m_pixelDeltaU;
    Vec3 m_pixelDeltaV;

    Vec3 m_viewportUpperLeft;
    Vec3 m_pixel00Loc;

    Renderer* m_renderer{nullptr};

    int m_samplesPerPixel;
    float m_pixelSamplesScale;
};
