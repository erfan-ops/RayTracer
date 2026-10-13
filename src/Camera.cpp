#include <Camera.hpp>
#include <Ray.hpp>
#include <HittableList.hpp>
#include <Common.hpp>
#include <Sphere.hpp>
#include <Interval.hpp>


double hit_sphere(const Point3& center, double radius, const Ray& r) {
    Vec3 oc = center - r.origin();
    double a = r.direction().length_squared();
    double h = dot(r.direction(), oc);
    double c = oc.length_squared() - radius*radius;
    double discriminant = h*h - a*c;

    if (discriminant < 0.0) {
        return -1.0;
    } else {
        return (h - std::sqrt(discriminant)) / a;
    }
}


Camera::Camera(
    const double focalLength,
    const Point3 &center,
    const size_t imageWidth,
    const size_t imageHeight,
    const int samplesPerPixel,
    const int maxDepth
) : m_focalLength(focalLength)
  , m_center(center)
  , m_renderer(new Renderer(imageWidth, imageHeight))
  , m_imageWidth(imageWidth)
  , m_imageHeight(imageHeight)
  , m_samplesPerPixel(samplesPerPixel)
  , m_maxDepth(maxDepth)
{
    m_viewportWidth = m_viewportHeight * static_cast<double>(imageWidth)/static_cast<double>(imageHeight);

    m_viewportU = Vec3(m_viewportWidth, 0.0, 0.0);
    m_viewportV = Vec3(0.0, -m_viewportHeight, 0.0);

    m_pixelDeltaU = m_viewportU / imageWidth;
    m_pixelDeltaV = m_viewportV / imageHeight;

    m_viewportUpperLeft = m_center - Vec3(0.0, 0.0, m_focalLength) - m_viewportU*0.5 - m_viewportV*0.5;
    m_pixel00Loc = m_viewportUpperLeft + 0.5 * (m_pixelDeltaU + m_pixelDeltaV);

    m_pixelSamplesScale = 1.0 / m_samplesPerPixel;
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

            Color pixel_color(0.0, 0.0, 0.0);
            for (int sample = 0; sample < m_samplesPerPixel; sample++) {
                Ray r = get_ray(i, j);
                pixel_color += ray_color(r, m_maxDepth, world);
            }
            m_renderer->write_color(m_pixelSamplesScale * pixel_color);
        }
    }

    return m_renderer->get_image();
}

Color Camera::ray_color(const Ray& r, const int depth, const Hittable& world) const {
    if (depth <= 0)
        return Color(0.0, 0.0, 0.0);
    
    HitRecord rec;
    if (world.hit(r, Interval(0.001, infinity), rec)) {
        Ray scattered;
        Color attenuation;
        if (rec.mat->scatter(r, rec, attenuation, scattered))
            return attenuation * ray_color(scattered, depth-1, world);
        return Color(0.0, 0.0, 0.0);
    }
    
    Vec3 unit_direction = unit_vector(r.direction());
    auto a = 0.5*(unit_direction.y() + 1.0);
    return (1.0-a)*Color(1.0, 1.0, 1.0) + a*Color(0.5, 0.7, 1.0);
}


static Vec3 sample_square() {
    return Vec3(random_double() - 0.5, random_double() - 0.5, 0);
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