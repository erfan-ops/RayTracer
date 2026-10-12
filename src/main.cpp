#include <iostream>

#include <Image.hpp>
#include <Renderer.hpp>
#include <Ray.hpp>
#include <Camera.hpp>


int main() {
    Image image{};

    const size_t width = 1280;
    const size_t height = 720;

    // Create camera
    Camera cam(1.0f, Point3(0.0f, 0.0f, 0.0f), width, height);

    // Render the image
    const unsigned char* data = cam.shot();

    // Save the image
    if (Image::isFormatAvailable(Image::Format::JPEG)) {
        image.savePNG(data, cam.width(), cam.height(), "output.jpg", Image::PixelFormat::RGB24);
    }
    else {
        std::cerr << "Format was unavailable" << std::endl;
    }

    return 0;
}