#include <Renderer.hpp>


Renderer::Renderer(size_t width, size_t height)
: m_width(width), m_height(height), m_imgSize(width * height * 3) {
    m_img = new unsigned char[m_imgSize];
}

void Renderer::write_color(const Color& pixel_color) {
    auto r = pixel_color.x();
    auto g = pixel_color.y();
    auto b = pixel_color.z();

    unsigned char rbyte = static_cast<unsigned char>(255.999 * r);
    unsigned char gbyte = static_cast<unsigned char>(255.999 * g);
    unsigned char bbyte = static_cast<unsigned char>(255.999 * b);

    m_img[m_idx++] = rbyte;
    m_img[m_idx++] = gbyte;
    m_img[m_idx++] = bbyte;
}

const unsigned char* Renderer::get_image() const { return m_img; }
const size_t Renderer::width() const { return m_width; }
const size_t Renderer::height() const { return m_height; }