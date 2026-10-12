#include <Camera.hpp>
#include <Ray.hpp>


Color ray_color(const Ray& r) {
    Vec3 unit_direction = unit_vector(r.direction());
    auto a = 0.5*(unit_direction.y() + 1.0);
    return (1.0-a)*Color(1.0, 1.0, 1.0) + a*Color(0.5, 0.7, 1.0);
}


Camera::Camera(
    const float focalLength,
    const Point3 &center,
    const size_t imageWidth,
    const size_t imageHeight
) : m_focalLength(focalLength)
  , m_center(center)
  , m_renderer(new Renderer(imageWidth, imageHeight))
  , m_imageWidth(imageWidth)
  , m_imageHeight(imageHeight)
{
    m_viewportWidth = m_viewportHeight * static_cast<float>(imageWidth)/static_cast<float>(imageHeight);

    m_viewportU = Vec3(m_viewportWidth, 0.0f, 0.0f);
    m_viewportV = Vec3(0.0f, -m_viewportHeight, 0.0f);

    m_pixelDeltaU = m_viewportU / imageWidth;
    m_pixelDeltaV = m_viewportV / imageHeight;

    m_viewportUpperLeft = m_center - Vec3(0.0f, 0.0f, m_focalLength) - m_viewportU*0.5 - m_viewportV*0.5;
    m_pixel00Loc = m_viewportUpperLeft + 0.5f * (m_pixelDeltaU + m_pixelDeltaV);
}


Camera::~Camera() {
    delete m_renderer;
}


const unsigned char* Camera::shot() const {
    for (int j = 0; j < m_imageHeight; j++) {
        for (int i = 0; i < m_imageWidth; i++) {
            auto pixel_center = m_pixel00Loc + (i * m_pixelDeltaU) + (j * m_pixelDeltaV);
            auto ray_direction = pixel_center - m_center;
            Ray r(m_center, ray_direction);

            Color pixel_color = ray_color(r);
            m_renderer->write_color(pixel_color);
        }
    }

    return m_renderer->get_image();
}


const size_t Camera::width() const { return m_imageWidth; }
const size_t Camera::height() const { return m_imageHeight; }