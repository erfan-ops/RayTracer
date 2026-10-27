#pragma once

#include <Vec3.hpp>
#include <atomic>
#include <vector>


class Renderer {
public:
    Renderer(size_t width, size_t height);

    void write_color(const Color& pixel_color, size_t idx);

    // Marks a scanline as finished. Camera calls this only after every pixel
    // in the row has been written, which lets a display thread read the row
    // without a lock: nobody writes to it again.
    void mark_row_complete(size_t row);

    // Copies row's width() * 3 bytes into dst if that row has been marked
    // complete. Returns false while the row is still being rendered.
    bool copy_row_if_complete(size_t row, unsigned char* dst) const;

    size_t completed_rows() const;

    const unsigned char* get_image() const;
    const size_t width() const;
    const size_t height() const;

private:
    size_t m_width;
    size_t m_height;

    std::vector<unsigned char> m_img;
    std::vector<std::atomic<bool>> m_rowComplete;
    std::atomic<size_t> m_completedRows{0};

    size_t m_idx{0};
};