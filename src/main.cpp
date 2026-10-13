#include <iostream>

#include <Image.hpp>
#include <Renderer.hpp>
#include <Ray.hpp>
#include <Camera.hpp>
#include <Sphere.hpp>


int main() {
    Image image{};

    const size_t width = 400;
    const size_t height = 225;

    // Create camera
    Camera cam(1.0f, Point3(0.0f, 0.0f, 0.0f), width, height, 100);

    // World
    HittableList world;
    world.add(std::make_shared<Sphere>(Point3(0,0,-1), 0.5));
    world.add(std::make_shared<Sphere>(Point3(0,-100.5,-1), 100));

    // Render the image
    const unsigned char* data = cam.shot(world);

    // Save the image
    if (Image::isFormatAvailable(Image::Format::PNG)) {
        image.savePNG(data, cam.width(), cam.height(), "out/output.png", Image::PixelFormat::RGB24);
    }
    else {
        std::cerr << "Format was unavailable" << std::endl;
    }

    return 0;
}