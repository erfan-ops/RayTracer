#pragma once

#include <Vec3.hpp>


class Renderer {
public:
    Renderer(size_t width, size_t height);

    void write_color(const Color& pixel_color);

    const unsigned char* get_image() const;
    const size_t width() const;
    const size_t height() const;

private:
    unsigned char* m_img;
    size_t m_imgSize;
    size_t m_idx{0};

    size_t m_width;
    size_t m_height;
};