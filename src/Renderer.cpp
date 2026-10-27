#include <Renderer.hpp>
#include <Interval.hpp>

#include <cstring>


Renderer::Renderer(
    size_t width,
    size_t height
) : m_width(width)
  , m_height(height)
  , m_img(width * height * 3)
  , m_rowComplete(height)
{
    for (auto& complete : m_rowComplete) {
        complete.store(false, std::memory_order_relaxed);
    }
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

void Renderer::mark_row_complete(size_t row) {
    if (row >= m_height) {
        return;
    }

    m_completedRows.fetch_add(1, std::memory_order_relaxed);
    m_rowComplete[row].store(true, std::memory_order_release);
}

bool Renderer::copy_row_if_complete(size_t row, unsigned char* dst) const {
    if (row >= m_height) {
        return false;
    }

    // Acquire pairs with the release store in mark_row_complete(), making all
    // pixel writes to this row visible before any of its bytes are copied.
    if (!m_rowComplete[row].load(std::memory_order_acquire)) {
        return false;
    }

    std::memcpy(dst, m_img.data() + row * m_width * 3, m_width * 3);
    return true;
}

size_t Renderer::completed_rows() const {
    return m_completedRows.load(std::memory_order_relaxed);
}

const unsigned char* Renderer::get_image() const { return m_img.data(); }
const size_t Renderer::width() const { return m_width; }
const size_t Renderer::height() const { return m_height; }