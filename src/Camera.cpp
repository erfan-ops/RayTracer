#include <Camera.hpp>
#include <Ray.hpp>
#include <HittableList.hpp>
#include <Common.hpp>
#include <Sphere.hpp>
#include <Interval.hpp>


float hit_sphere(const Point3& center, float radius, const Ray& r) {
    Vec3 oc = center - r.origin();
    float a = r.direction().length_squared();
    float h = dot(r.direction(), oc);
    float c = oc.length_squared() - radius*radius;
    float discriminant = h*h - a*c;

    if (discriminant < 0.0f) {
        return -1.0f;
    } else {
        return (h - std::sqrt(discriminant)) / a;
    }
}

Color ray_color(const Ray& r, const Hittable& world) {
    HitRecord rec;
    if (world.hit(r, Interval(0, infinity), rec)) {
        return 0.5 * (rec.normal + Color(1,1,1));
    }
    
    Vec3 unit_direction = unit_vector(r.direction());
    auto a = 0.5f*(unit_direction.y() + 1.0f);
    return (1.0f-a)*Color(1.0f, 1.0f, 1.0f) + a*Color(0.5f, 0.7f, 1.0f);
}


Camera::Camera(
    const float focalLength,
    const Point3 &center,
    const size_t imageWidth,
    const size_t imageHeight,
    const int samplesPerPixel
) : m_focalLength(focalLength)
  , m_center(center)
  , m_renderer(new Renderer(imageWidth, imageHeight))
  , m_imageWidth(imageWidth)
  , m_imageHeight(imageHeight)
  , m_samplesPerPixel(samplesPerPixel)
{
    m_viewportWidth = m_viewportHeight * static_cast<float>(imageWidth)/static_cast<float>(imageHeight);

    m_viewportU = Vec3(m_viewportWidth, 0.0f, 0.0f);
    m_viewportV = Vec3(0.0f, -m_viewportHeight, 0.0f);

    m_pixelDeltaU = m_viewportU / imageWidth;
    m_pixelDeltaV = m_viewportV / imageHeight;

    m_viewportUpperLeft = m_center - Vec3(0.0f, 0.0f, m_focalLength) - m_viewportU*0.5 - m_viewportV*0.5;
    m_pixel00Loc = m_viewportUpperLeft + 0.5f * (m_pixelDeltaU + m_pixelDeltaV);

    m_pixelSamplesScale = 1.0f / m_samplesPerPixel;
}


Camera::~Camera() {
    delete m_renderer;
}


const unsigned char* Camera::shot(HittableList world) const {
    for (int j = 0; j < m_imageHeight; j++) {
        for (int i = 0; i < m_imageWidth; i++) {
            auto pixel_center = m_pixel00Loc + (i * m_pixelDeltaU) + (j * m_pixelDeltaV);
            auto ray_direction = pixel_center - m_center;
            Ray r(m_center, ray_direction);

            Color pixel_color(0.0f, 0.0f, 0.0f);
            for (int sample = 0; sample < m_samplesPerPixel; sample++) {
                Ray r = get_ray(i, j);
                pixel_color += ray_color(r, world);
            }
            m_renderer->write_color(m_pixelSamplesScale * pixel_color);
        }
    }

    return m_renderer->get_image();
}


static Vec3 sample_square() {
    return Vec3(random_float() - 0.5, random_float() - 0.5, 0);
    // return Vec3(0.5, 0.5, 0);
}

Ray Camera::get_ray(int i, int j) const {
    auto offset = sample_square();
    auto pixel_sample = m_pixel00Loc
                        + ((i + offset.x()) * m_pixelDeltaU)
                        + ((j + offset.y()) * m_pixelDeltaV);

    auto ray_origin = m_center;
    auto ray_direction = pixel_sample - ray_origin;

    return Ray(ray_origin, ray_direction);
}


const size_t Camera::width() const { return m_imageWidth; }
const size_t Camera::height() const { return m_imageHeight; }