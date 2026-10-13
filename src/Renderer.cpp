#include <Renderer.hpp>
#include <Interval.hpp>


Renderer::Renderer(size_t width, size_t height)
: m_width(width), m_height(height), m_imgSize(width * height * 3) {
    m_img = new unsigned char[m_imgSize];
}

static inline double linear_to_gamma(double linear_component)
{
    if (linear_component > 0)
        return std::sqrt(linear_component);

    return 0;
}

void Renderer::write_color(const Color& pixel_color, size_t idx) {
    auto r = linear_to_gamma(pixel_color.r());
    auto g = linear_to_gamma(pixel_color.g());
    auto b = linear_to_gamma(pixel_color.b());

    static const Interval intensity(0.000, 0.999);
    unsigned char rbyte = static_cast<unsigned char>(256.0 * intensity.clamp(r));
    unsigned char gbyte = static_cast<unsigned char>(256.0 * intensity.clamp(g));
    unsigned char bbyte = static_cast<unsigned char>(256.0 * intensity.clamp(b));

    m_img[idx] = rbyte;
    m_img[idx + 1] = gbyte;
    m_img[idx + 2] = bbyte;
}

const unsigned char* Renderer::get_image() const { return m_img; }
const size_t Renderer::width() const { return m_width; }
const size_t Renderer::height() const { return m_height; }