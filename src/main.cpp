#include <iostream>

#include <Image.hpp>

int main() {
    Image image{};

    const int width = 1920;
    const int height = 1080;
    const int size = width * height;
    unsigned char* data = new unsigned char[size];

    for (int i = 0; i < size; i++) {
        int x = i % width;
        int y = i / width;

        float r, g, b;
        r = (static_cast<float>(x) / static_cast<float>(width));
        g = (1.0f - (static_cast<float>(y) / static_cast<float>(height)));
        b = 1.0f;

        unsigned char m = static_cast<unsigned char>((r * 0.299f + g * 0.587f + b * 0.114) * 255.0f);
        data[i] = m;
    }

    if (Image::isFormatAvailable(Image::Format::JPEG)) {
        image.savePNG(data, width, height, "output.jpg", Image::PixelFormat::GRAY8);
    }
    else {
        std::cerr << "Format was unavailable" << std::endl;
    }

    return 0;
}