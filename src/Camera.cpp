#include <Camera.hpp>
#include <Ray.hpp>
#include <HittableList.hpp>
#include <Common.hpp>
#include <Interval.hpp>
#include <Material.hpp>

#include <thread>


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
    const double verticalFOV,
    const Point3& lookfrom,
    const Point3& lookat,
    const Vec3& vup,
    const size_t imageWidth,
    const size_t imageHeight,
    const int samplesPerPixel,
    const int maxDepth,
    const double defocusAngle,
    const double focusDist,
    const Color& backgroundColor
) : m_vfov(verticalFOV)
  , m_center(lookfrom)
  , m_renderer(new Renderer(imageWidth, imageHeight))
  , m_imageWidth(imageWidth)
  , m_imageHeight(imageHeight)
  , m_samplesPerPixel(samplesPerPixel)
  , m_maxDepth(maxDepth)
  , m_defocusAngle(defocusAngle)
  , m_focusDist(focusDist)
  , m_bgColor(backgroundColor)
{
    w = unit_vector(lookfrom - lookat);
    u = unit_vector(cross(vup, w));
    v = cross(w, u);

    double theta = degrees_to_radians(m_vfov);
    double h = std::tan(theta/2.0);
    m_viewportHeight = 2.0 * h * m_focusDist;
    m_viewportWidth = m_viewportHeight * static_cast<double>(imageWidth)/static_cast<double>(imageHeight);

    m_viewportU = m_viewportWidth * u;
    m_viewportV = m_viewportHeight * -v;

    m_pixelDeltaU = m_viewportU / imageWidth;
    m_pixelDeltaV = m_viewportV / imageHeight;

    m_viewportUpperLeft = m_center - (m_focusDist * w) - m_viewportU*0.5 - m_viewportV*0.5;
    m_pixel00Loc = m_viewportUpperLeft + 0.5 * (m_pixelDeltaU + m_pixelDeltaV);

    double defocus_radius = m_focusDist * std::tan(degrees_to_radians(m_defocusAngle * 0.5));
    m_defocusDiskU = u * defocus_radius;
    m_defocusDiskV = v * defocus_radius;

    m_sqrtSpp = int(std::sqrt(m_samplesPerPixel));
    m_pixelSamplesScale = 1.0 / (m_sqrtSpp * m_sqrtSpp);
    m_recipSqrtSpp = 1.0 / m_sqrtSpp;
}


Camera::~Camera() {
    delete m_renderer;
}


const unsigned char* Camera::shot(
    HittableList world,
    const std::atomic<bool>* cancel
) const {
    constexpr int nThreads = 12;

    std::vector<std::thread> threads;
    threads.reserve(nThreads);

    for (int threadIdx = 0; threadIdx < nThreads; threadIdx++) {
        threads.emplace_back([this, &world, threadIdx, nThreads, cancel]() {
            for (int j = threadIdx; j < m_imageHeight; j += nThreads) {
                if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) {
                    return;
                }

                std::cout << '\r' << j << "                    ";

                for (int i = 0; i < m_imageWidth; i++) {
                    if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) {
                        return;
                    }

                    auto pixel_center =
                        m_pixel00Loc + (i * m_pixelDeltaU) + (j * m_pixelDeltaV);

                    auto ray_direction = pixel_center - m_center;

                    Color pixel_color(0.0, 0.0, 0.0);

                    for (int s_j = 0; s_j < m_sqrtSpp; s_j++) {
                        for (int s_i = 0; s_i < m_sqrtSpp; s_i++) {
                            Ray r = get_ray(i, j, s_i, s_j);
                            pixel_color += ray_color(r, m_maxDepth, world);
                        }
                    }

                    m_renderer->write_color(m_pixelSamplesScale * pixel_color, 3ull * (j * m_imageWidth + i));
                }

                m_renderer->mark_row_complete(static_cast<size_t>(j));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return m_renderer->get_image();
}

Color Camera::ray_color(const Ray& r, const int depth, const Hittable& world) const {
    if (depth <= 0)
        return Color(0.0, 0.0, 0.0);
    
    HitRecord rec;

    if (!world.hit(r, Interval(0.001, infinity), rec))
        return m_bgColor;

    Ray scattered;
    Color attenuation;
    Color color_from_emission = rec.mat->emitted(rec.u, rec.v, rec.p);

    if (!rec.mat->scatter(r, rec, attenuation, scattered))
        return color_from_emission;

    double scattering_pdf = rec.mat->scattering_pdf(r, rec, scattered);
    double pdf_value = scattering_pdf;

    Color color_from_scatter = (attenuation * scattering_pdf * ray_color(scattered, depth-1, world)) / pdf_value;

    return color_from_emission + color_from_scatter;
}


static Vec3 sample_square() {
    return Vec3(random_double() - 0.5, random_double() - 0.5, 0);
}

Point3 Camera::defocus_disk_sample() const {
    Vec3 p = random_in_unit_disk();
    return m_center + (p[0] * m_defocusDiskU) + (p[1] * m_defocusDiskV);
}

Ray Camera::get_ray(int i, int j, int s_i, int s_j) const {
    auto offset = sample_square_stratified(s_i, s_j);
    Vec3 pixel_sample = m_pixel00Loc
                        + ((i + offset.x()) * m_pixelDeltaU)
                        + ((j + offset.y()) * m_pixelDeltaV);

    Vec3 ray_origin = (m_defocusAngle <= 0) ? m_center : defocus_disk_sample();
    Vec3 ray_direction = pixel_sample - ray_origin;
    double ray_time = random_double();

    return Ray(ray_origin, ray_direction, ray_time);
}

Vec3 Camera::sample_square_stratified(int s_i, int s_j) const {
    auto px = ((s_i + random_double()) * m_recipSqrtSpp) - 0.5;
    auto py = ((s_j + random_double()) * m_recipSqrtSpp) - 0.5;

    return Vec3(px, py, 0);
}


const Renderer& Camera::renderer() const { return *m_renderer; }

const size_t Camera::width() const { return m_imageWidth; }
const size_t Camera::height() const { return m_imageHeight; }