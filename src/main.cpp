#include <iostream>
#include <atomic>
#include <filesystem>
#include <thread>
#include <utility>

#include <Image.hpp>
#include <Renderer.hpp>
#include <Ray.hpp>
#include <Camera.hpp>
#include <PreviewWindow.hpp>
#include <hittables/Sphere.hpp>
#include <hittables/Quad.hpp>
#include <hittables/Triangle.hpp>
#include <hittables/ConstantMedium.hpp>
#include <BVH.hpp>
#include <Texture.hpp>


// Renders the scene on a worker thread while a native window shows the image
// as scanlines complete, then writes the PNG. Closing the window early cancels
// the render and leaves the previous output file untouched.
void render_with_preview(
    const Camera& cam,
    HittableList world,
    const std::filesystem::path& output_path
) {
    PreviewWindow preview{cam.renderer()};

    std::atomic<bool> cancel{false};

    std::thread render_thread(
        [&cam, world = std::move(world), &cancel, output_path]() mutable {
            const unsigned char* data = cam.shot(std::move(world), &cancel);

            // A cancelled render is incomplete and must not replace a good
            // image on disk.
            if (cam.renderer().completed_rows() != cam.height()) {
                return;
            }

            ImageWrite image{};
            if (ImageWrite::isFormatAvailable(ImageWrite::Format::PNG)) {
                image.savePNG(data, cam.width(), cam.height(), output_path,
                              ImageWrite::PixelFormat::RGB24);
            }
            else {
                std::cerr << "Format was unavailable" << std::endl;
            }
        });

    preview.run();

    cancel.store(true, std::memory_order_relaxed);
    render_thread.join();
}


void cornell_box4() {
    HittableList world;

    auto white  = std::make_shared<Lambertian>(Color(.8, .8, .8));
    auto light  = std::make_shared<DiffuseLight>(Color(40, 20, 15));

    world.add(
        std::make_shared<Quad>(
            Point3(555,0,0), Vec3(0,555,0), Vec3(0,0,555),
            std::make_shared<Lambertian>(
                std::make_shared<ImageTexture>(
                    "textures/WhiteTiles07_MR_4K/WhiteTiles07_4K_BaseColor.png",
                    "textures/WhiteTiles07_MR_4K/WhiteTiles07_4K_AO.png"
                )
            )
        )
    ); // left wall

    world.add(
        std::make_shared<Quad>(
            Point3(0,0,0), Vec3(0,555,0), Vec3(0,0,555),
            std::make_shared<Lambertian>(
                std::make_shared<ImageTexture>(
                    "textures/white-square-tiles-pbr-2k/white-square-tiles_2k_color.jpg",
                    "textures/white-square-tiles-pbr-2k/white-square-tiles_2k_ambientocclusion.jpg"
                )
            )
        )
    ); // right wall

    world.add(
        std::make_shared<Quad>(
            Point3(0,0,555), Vec3(555,0,0), Vec3(0,555,0),
            std::make_shared<Lambertian>(
                std::make_shared<ImageTexture>(
                    "textures/long-white-tiles/long_white_tiles_diff_4k.jpg",
                    "textures/long-white-tiles/long_white_tiles_ao_4k.jpg"
                )
            )
        )
    ); // back wall

    world.add(std::make_shared<Quad>(Point3(0,0,0), Vec3(555,0,0), Vec3(0,0,555), std::make_shared<Metal>(Color(0.5, 0.5, 0.5), 0.0))); // floor

    world.add(std::make_shared<Quad>(Point3(555,555,555), Vec3(-555,0,0), Vec3(0,0,-555), std::make_shared<Lambertian>(std::make_shared<CheckerTexture>(150, Color(1.0, 1.0, 1.0), Color(0.7, 0.7, 0.7))))); // ceiling

    world.add(std::make_shared<Quad>(Point3(203,554,203), Vec3(150,0,0), Vec3(0,0,150), std::make_shared<DiffuseLight>(Color(3.0, 3.0, 3.0)), 20)); // light source


    double r = 100;
    world.add(std::make_shared<Sphere>(Point3(278, 278, 278), r, std::make_shared<DiffuseLight>(Color(0.8, 2.0, 2.5))));

    size_t nBalls = 8;
    double radius = 125;
    double ox = 278;
    double oz = 278 + radius;
    for (size_t i = 0; i < nBalls; i++) {
        // linear interpolation
        double angle = std::numbers::pi * 2.0 * i / nBalls;

        double lx = ox - 278;
        double lz = oz - 278;
        double x = lx * std::cos(angle) - lz * std::sin(angle) + 278;
        double z = lx * std::sin(angle) + lz * std::cos(angle) + 278;

        // world.add(std::make_shared<Sphere>(Point3(x, 278, z), 25, std::make_shared<Dielectric>(1.5)));
    }

    double oy = 278 + radius;
    for (size_t i = 0; i < nBalls; i++) {
        if (i == 2 || i == 6)
            continue;
        // linear interpolation
        double angle = std::numbers::pi * 2.0 * i / nBalls;

        double lx = ox - 278;
        double ly = oy - 278;
        double x = lx * std::cos(angle) - ly * std::sin(angle) + 278;
        double y = lx * std::sin(angle) + ly * std::cos(angle) + 278;

        // world.add(std::make_shared<Sphere>(Point3(x, y, 278), 25, std::make_shared<Dielectric>(1.5)));
    }

    oy = 278;
    for (size_t i = 0; i < nBalls; i++) {
        if (!(i & 1))
            continue;
        // linear interpolation
        double angle = std::numbers::pi * 2.0 * i / nBalls;

        double lz = oz - 278;
        double ly = oy - 278;
        double z = lz * std::cos(angle) - ly * std::sin(angle) + 278;
        double y = lz * std::sin(angle) + ly * std::cos(angle) + 278;

        // world.add(std::make_shared<Sphere>(Point3(278, y, z), 25, std::make_shared<Dielectric>(1.5)));
    }


    // second layer
    radius = 200;
    ox = 278;
    oz = 278 + radius;
    for (size_t i = 0; i < nBalls; i++) {
        // linear interpolation
        double angle = std::numbers::pi * 2.0 * i / nBalls;

        double lx = ox - 278;
        double lz = oz - 278;
        double x = lx * std::cos(angle) - lz * std::sin(angle) + 278;
        double z = lx * std::sin(angle) + lz * std::cos(angle) + 278;

        // world.add(std::make_shared<Sphere>(Point3(x, 278, z), 25, std::make_shared<Dielectric>(1.5)));
    }

    oy = 278 + radius;
    for (size_t i = 0; i < nBalls; i++) {
        if (i == 2 || i == 6)
            continue;
        // linear interpolation
        double angle = std::numbers::pi * 2.0 * i / nBalls;

        double lx = ox - 278;
        double ly = oy - 278;
        double x = lx * std::cos(angle) - ly * std::sin(angle) + 278;
        double y = lx * std::sin(angle) + ly * std::cos(angle) + 278;

        // world.add(std::make_shared<Sphere>(Point3(x, y, 278), 25, std::make_shared<Dielectric>(1.5)));
    }

    oy = 278;
    for (size_t i = 0; i < nBalls; i++) {
        if (!(i & 1))
            continue;
        // linear interpolation
        double angle = std::numbers::pi * 2.0 * i / nBalls;

        double lz = oz - 278;
        double ly = oy - 278;
        double z = lz * std::cos(angle) - ly * std::sin(angle) + 278;
        double y = lz * std::sin(angle) + ly * std::cos(angle) + 278;

        // world.add(std::make_shared<Sphere>(Point3(278, y, z), 25, std::make_shared<Dielectric>(1.5)));
    }


    const size_t width = 400ull;
    const size_t height = 400ull;

    Camera cam(
        40.0,
        Point3(150, 380, -650),
        Point3(278, 278, 278),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        300,
        50,
        0.0,
        100.0,
        Color(0.0, 0.0, 0.0)
    );

    render_with_preview(cam, std::move(world), "out/output.png");
}


int main() {
    switch (7) {
        case 7: cornell_box4(); break;
    }

    return 0;
}