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


void simple_light() {
    HittableList world;

    auto pertext = std::make_shared<NoiseTexture>(4);
    world.add(std::make_shared<Sphere>(Point3(0,-1000,0), 1000, std::make_shared<Lambertian>(pertext)));
    world.add(std::make_shared<Sphere>(Point3(0,2,0), 2, std::make_shared<Lambertian>(pertext)));

    auto difflight = std::make_shared<DiffuseLight>(Color(4,4,4));
    world.add(std::make_shared<Sphere>(Point3(0.0, 7.0, 0.0), 2.0, difflight));
    world.add(std::make_shared<Quad>(Point3(3,1,-2), Vec3(2,0,0), Vec3(0,2,0), difflight));

    const size_t width = 1280ull;
    const size_t height = 720ull;

    Camera cam(
        20.0,
        Point3(26.0, 3.0, 6.0),
        Point3(0.0, 2.0, 0.0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        800,
        80,
        0.0,
        10.0,
        Color(0.0, 0.0, 0.0)
    );

    render_with_preview(cam, std::move(world), "out/output.png");
}


void cornell_box() {
    HittableList world;

    auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
    auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
    auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
    auto light = std::make_shared<DiffuseLight>(Color(15, 15, 15));

    world.add(std::make_shared<Quad>(Point3(555,0,0),     Vec3(0,555,0),  Vec3(0,0,555), green));
    world.add(std::make_shared<Quad>(Point3(0,0,0),       Vec3(0,555,0),  Vec3(0,0,555), red));
    world.add(std::make_shared<Quad>(Point3(343,554,332), Vec3(-130,0,0), Vec3(0,0,-105), light)); // light source
    world.add(std::make_shared<Quad>(Point3(0,0,0),       Vec3(555,0,0),  Vec3(0,0,555), white));
    world.add(std::make_shared<Quad>(Point3(555,555,555), Vec3(-555,0,0), Vec3(0,0,-555), white));
    world.add(std::make_shared<Quad>(Point3(0,0,555),     Vec3(555,0,0),  Vec3(0,555,0), white));

    std::shared_ptr<Hittable> box1 = box(Point3(0,165,0), Point3(165,330,165), white);
    box1 = std::make_shared<RotateY>(box1, 15);
    box1 = std::make_shared<Translate>(box1, Vec3(265,0,295));
    world.add(box1);

    std::shared_ptr<Hittable> box2 = box(Point3(0,0,0), Point3(165,165,165), white);
    box2 = std::make_shared<RotateY>(box2, -18);
    box2 = std::make_shared<Translate>(box2, Vec3(130,0,65));
    world.add(box2);

    double r = 50;
    world.add(std::make_shared<Sphere>(Point3(165, 165 + r, 165), r, std::make_shared<Dielectric>(1.5)));

    r = 150;
    world.add(std::make_shared<Sphere>(Point3(265+82.5, 247.5, 295+82.5), r, std::make_shared<Dielectric>(1.5)));

    const size_t width = 500ull;
    const size_t height = 500ull;

    Camera cam(
        40.0,
        Point3(278, 278, -800),
        Point3(278, 278, 0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        500,
        50,
        0.0,
        10.0,
        Color(0.0, 0.0, 0.0)
    );

    render_with_preview(cam, std::move(world), "out/output.png");
}


void cornell_box2() {
    HittableList world;

    auto red    = std::make_shared<Lambertian>(Color(.75, .05, .05));
    auto purple = std::make_shared<Lambertian>(Color(.4, .05, .7));
    auto white  = std::make_shared<Lambertian>(Color(.83, .83, .83));
    auto black  = std::make_shared<Lambertian>(Color(.03, .03, .03));
    auto green  = std::make_shared<Lambertian>(Color(.05, .55, .05));
    auto light  = std::make_shared<DiffuseLight>(Color(40, 20, 15));
    auto minilight = std::make_shared<DiffuseLight>(Color(200, 90, 50));
    auto backwall = std::make_shared<Metal>(Color(0.3, 0.3, 0.3), 0.01);

    world.add(std::make_shared<Quad>(Point3(555,0,0),     Vec3(0,555,0),  Vec3(0,0,555), red));
    world.add(std::make_shared<Quad>(Point3(0,0,0),       Vec3(0,555,0),  Vec3(0,0,555), purple));
    // world.add(std::make_shared<Quad>(Point3(343,554,332), Vec3(-130,0,0), Vec3(0,0,-105), light, 20)); // light source
    world.add(std::make_shared<Quad>(Point3(-5000,0,-5000), Vec3(10555,0,0),  Vec3(0,0,10555), green));
    world.add(std::make_shared<Quad>(Point3(555,555,555), Vec3(-555,0,0), Vec3(0,0,-555), white));
    world.add(std::make_shared<Quad>(Point3(0,0,555),     Vec3(555,0,0),  Vec3(0,555,0), backwall));
    // world.add(std::make_shared<Quad>(Point3(0, 0, 0),     Vec3(555, 0, 0),  Vec3(0,555,0), backwall));

    double r = 12;
    world.add(std::make_shared<Sphere>(Point3(16, 350, 277.5), r, minilight));
    world.add(std::make_shared<Sphere>(Point3(555-16, 350, 277.5), r, minilight));

    double lamp_ratio = 2;
    world.add(std::make_shared<Quad>(Point3(r*2.0 + 10, 350-r*2.0*lamp_ratio, 277.5-r*lamp_ratio), Vec3(0,r*4.0*lamp_ratio,0),  Vec3(0,0,r*2.0*lamp_ratio), black));
    world.add(std::make_shared<Triangle>(
        Point3(r*2.0 + 10, 350-r*2.0*lamp_ratio, 277.5-r*lamp_ratio),
        Point3(r*2.0 + 10, 350-r*2.0*lamp_ratio + r*4.0*lamp_ratio, 277.5-r*lamp_ratio),
        Point3((r*2.0 + 10) - (r*2.0*lamp_ratio), 350-r*2.0*lamp_ratio + r*2.0*lamp_ratio, 277.5-r*lamp_ratio),
        black
    ));
    world.add(std::make_shared<Triangle>(
        Point3(r*2.0 + 10, 350-r*2.0*lamp_ratio, 277.5+r*lamp_ratio),
        Point3(r*2.0 + 10, 350-r*2.0*lamp_ratio + r*4.0*lamp_ratio, 277.5+r*lamp_ratio),
        Point3((r*2.0 + 10) - (r*2.0*lamp_ratio), 350-r*2.0*lamp_ratio + r*2.0*lamp_ratio, 277.5+r*lamp_ratio),
        black
    ));


    world.add(std::make_shared<Quad>(Point3(555 - (r*2.0 + 10), 350-r*2.0*lamp_ratio, 277.5-r*lamp_ratio), Vec3(0,r*4.0*lamp_ratio,0),  Vec3(0,0,r*2.0*lamp_ratio), black));
    world.add(std::make_shared<Triangle>(
        Point3(555 - (r*2.0 + 10), 350-r*2.0*lamp_ratio, 277.5-r*lamp_ratio),
        Point3(555 - (r*2.0 + 10), 350-r*2.0*lamp_ratio + r*4.0*lamp_ratio, 277.5-r*lamp_ratio),
        Point3((555 - (r*2.0 + 10)) + (r*2.0*lamp_ratio), 350-r*2.0*lamp_ratio + r*2.0*lamp_ratio, 277.5-r*lamp_ratio),
        black
    ));
    world.add(std::make_shared<Triangle>(
        Point3(555 - (r*2.0 + 10), 350-r*2.0*lamp_ratio, 277.5+r*lamp_ratio),
        Point3(555 - (r*2.0 + 10), 350-r*2.0*lamp_ratio + r*4.0*lamp_ratio, 277.5+r*lamp_ratio),
        Point3((555 - (r*2.0 + 10)) + (r*2.0*lamp_ratio), 350-r*2.0*lamp_ratio + r*2.0*lamp_ratio, 277.5+r*lamp_ratio),
        black
    ));


    auto box1Mat = std::make_shared<Metal>(Color(0.30, 0.30, 0.30), 0.3);
    std::shared_ptr<Hittable> box1 = box(Point3(0,0,0), Point3(165,165,165), box1Mat);
    box1 = std::make_shared<RotateY>(box1, 25);
    box1 = std::make_shared<Translate>(box1, Vec3(265,0,200));
    world.add(box1);

    auto ballMat = std::make_shared<Metal>(Color(0.35, 0.35, 0.35), 0.0);
    Point3 localCenter(82.5, 165.0 + 82.5, 82.5);

    // Rotate the local point by 30 degrees, then translate.
    double angle = degrees_to_radians(25.0);

    Point3 sphereCenter(
        265 + std::cos(angle) * localCenter.x() +
            std::sin(angle) * localCenter.z(),
        localCenter.y(),
        200 - std::sin(angle) * localCenter.x() +
            std::cos(angle) * localCenter.z()
    );
    world.add(std::make_shared<Sphere>(sphereCenter, 82.5, std::make_shared<Dielectric>(1.5)));


    world.add(std::make_shared<Sphere>(Point3(150,70,145), 70, ballMat));


    const size_t width = 256ull;
    const size_t height = 256ull;

    Camera cam(
        70.0,
        Point3(278, 278, -750),
        Point3(278, 278, 278),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        200,
        30,
        0.0,
        100.0,
        Color(0.8, 0.8, 0.9)
    );

    render_with_preview(cam, std::move(world), "out/output.png");
}


void cornell_smoke() {
    HittableList world;

    auto red   = std::make_shared<Lambertian>(Color(.65, .05, .05));
    auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
    auto green = std::make_shared<Lambertian>(Color(.12, .45, .15));
    auto light = std::make_shared<DiffuseLight>(Color(7, 7, 7));

    world.add(std::make_shared<Quad>(Point3(555,0,0), Vec3(0,555,0), Vec3(0,0,555), green));
    world.add(std::make_shared<Quad>(Point3(0,0,0), Vec3(0,555,0), Vec3(0,0,555), red));
    world.add(std::make_shared<Quad>(Point3(113,554,127), Vec3(330,0,0), Vec3(0,0,305), light));
    world.add(std::make_shared<Quad>(Point3(0,555,0), Vec3(555,0,0), Vec3(0,0,555), white));
    world.add(std::make_shared<Quad>(Point3(0,0,0), Vec3(555,0,0), Vec3(0,0,555), white));
    world.add(std::make_shared<Quad>(Point3(0,0,555), Vec3(555,0,0), Vec3(0,555,0), white));

    std::shared_ptr<Hittable> box1 = box(Point3(0,0,0), Point3(165,330,165), white);
    box1 = make_shared<RotateY>(box1, 15);
    box1 = make_shared<Translate>(box1, Vec3(265,0,295));

    std::shared_ptr<Hittable> box2 = box(Point3(0,0,0), Point3(165,165,165), white);
    box2 = make_shared<RotateY>(box2, -18);
    box2 = make_shared<Translate>(box2, Vec3(130,0,65));

    world.add(std::make_shared<ConstantMedium>(box1, 0.05, Color(0,0,0)));
    world.add(std::make_shared<ConstantMedium>(box2, 0.05, Color(1,1,1)));

    const size_t width = 600ull;
    const size_t height = 600ull;

    Camera cam(
        40.0,
        Point3(278, 278, -800),
        Point3(278, 278, 0),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        500,
        50,
        0.0,
        10.0,
        Color(0.0, 0.0, 0.0)
    );

    render_with_preview(cam, std::move(world), "out/output.png");
}


void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    HittableList boxes1;
    auto ground = std::make_shared<Lambertian>(Color(0.48, 0.83, 0.53));

    int boxes_per_side = 9;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0;
            auto x0 = -200.0 + i*w;
            auto z0 = -400.0 + j*w;
            auto y0 = 0.0;
            auto x1 = x0 + w;
            auto y1 = random_double(1,101);
            auto z1 = z0 + w;

            boxes1.add(box(Point3(x0,y0,z0), Point3(x1,y1,z1), ground));
        }
    }

    HittableList world;

    world.add(std::make_shared<BVHNode>(boxes1));

    auto light = std::make_shared<DiffuseLight>(Color(7, 7, 7));
    world.add(std::make_shared<Quad>(Point3(123,554,147), Vec3(300,0,0), Vec3(0,0,265), light));

    world.add(std::make_shared<Sphere>(Point3(260, 150, 45), 50, std::make_shared<Dielectric>(1.5)));

    const size_t width = image_width;
    const size_t height = image_width;

    Camera cam(
        30.0,
        Point3(478, 278, -600),
        Point3(260, 150, 45),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        samples_per_pixel,
        max_depth,
        0.0,
        10.0,
        Color(0.0, 0.0, 0.0)
    );

    render_with_preview(cam, std::move(world), "out/output.png");
}


void cornell_box3() {
    HittableList world;

    auto red   = std::make_shared<Lambertian>(Color(.75, .05, .05));
    auto blue  = std::make_shared<Lambertian>(Color(.0, .4, .7));
    auto white = std::make_shared<Lambertian>(Color(.73, .73, .73));
    auto light = std::make_shared<DiffuseLight>(Color(40, 40, 40));

    world.add(std::make_shared<Quad>(Point3(555,0,0),     Vec3(0,555,0),  Vec3(0,0,555), red));
    world.add(std::make_shared<Quad>(Point3(0,0,0),       Vec3(0,555,0),  Vec3(0,0,555), blue));
    world.add(std::make_shared<Quad>(Point3(313,554,302), Vec3(-100,0,0), Vec3(0,0,-75), light, 20)); // light source
    world.add(std::make_shared<Quad>(Point3(0,0,0),       Vec3(555,0,0),  Vec3(0,0,555), white));
    world.add(std::make_shared<Quad>(Point3(555,555,555), Vec3(-555,0,0), Vec3(0,0,-555), white));
    world.add(std::make_shared<Quad>(Point3(0,0,555),     Vec3(555,0,0),  Vec3(0,555,0), white));


    std::shared_ptr<Hittable> box1 = box(Point3(0,0,0), Point3(165,330,165), white);
    box1 = std::make_shared<RotateY>(box1, 45);
    box1 = std::make_shared<Translate>(box1, Vec3(60,0,250));
    world.add(box1);

    world.add(std::make_shared<Sphere>(Point3(410, 165.0, 250), 82.5, std::make_shared<Dielectric>(1.5)));


    const size_t width = 512ull;
    const size_t height = 512ull;

    Camera cam(
        67.0,
        Point3(278, 278, -400),
        Point3(278, 278, 500),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        500,
        50,
        0.0,
        100.0,
        Color(0, 0, 0)
    );

    render_with_preview(cam, std::move(world), "out/output.png");
}


void cornell_box4() {
    HittableList world;

    auto white  = std::make_shared<Lambertian>(Color(.8, .8, .8));
    auto light  = std::make_shared<DiffuseLight>(Color(40, 20, 15));

    world.add(std::make_shared<Quad>(Point3(555,0,0), Vec3(0,555,0), Vec3(0,0,555), white)); // left wall
    world.add(std::make_shared<Quad>(Point3(554,0,0), Vec3(0,555,0), Vec3(0,0,555), std::make_shared<Dielectric>(1.5)));

    world.add(std::make_shared<Quad>(Point3(0,0,0), Vec3(0,555,0), Vec3(0,0,555), white)); // right wall
    world.add(std::make_shared<Quad>(Point3(1,0,0), Vec3(0,555,0), Vec3(0,0,555), std::make_shared<Dielectric>(1.5)));

    world.add(std::make_shared<Quad>(Point3(0,0,0), Vec3(555,0,0), Vec3(0,0,555), std::make_shared<Metal>(Color(0.5, 0.5, 0.5), 0.0))); // floor

    world.add(std::make_shared<Quad>(Point3(555,555,555), Vec3(-555,0,0), Vec3(0,0,-555), std::make_shared<Lambertian>(std::make_shared<CheckerTexture>(150, Color(1.0, 1.0, 1.0), Color(0.7, 0.7, 0.7))))); // ceiling

    world.add(std::make_shared<Quad>(Point3(0,0,555), Vec3(555,0,0), Vec3(0,555,0), white)); // back wall
    world.add(std::make_shared<Quad>(Point3(0,0,554), Vec3(555,0,0), Vec3(0,555,0), std::make_shared<Dielectric>(1.5)));

    // world.add(std::make_shared<Quad>(Point3(203,554,203), Vec3(150,0,0), Vec3(0,0,150), std::make_shared<DiffuseLight>(Color(3.0, 3.0, 3.0)), 20)); // light source


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

        world.add(std::make_shared<Sphere>(Point3(x, 278, z), 25, std::make_shared<Dielectric>(1.5)));
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

        world.add(std::make_shared<Sphere>(Point3(x, y, 278), 25, std::make_shared<Dielectric>(1.5)));
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

        world.add(std::make_shared<Sphere>(Point3(278, y, z), 25, std::make_shared<Dielectric>(1.5)));
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

        world.add(std::make_shared<Sphere>(Point3(x, 278, z), 25, std::make_shared<Dielectric>(1.5)));
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

        world.add(std::make_shared<Sphere>(Point3(x, y, 278), 25, std::make_shared<Dielectric>(1.5)));
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

        world.add(std::make_shared<Sphere>(Point3(278, y, z), 25, std::make_shared<Dielectric>(1.5)));
    }


    const size_t width = 400ull;
    const size_t height = 400ull;

    Camera cam(
        40.0,
        Point3(150, 380, -660),
        Point3(278, 278, 278),
        Vec3(0.0, 1.0, 0.0),
        width,
        height,
        300,
        30,
        0.0,
        100.0,
        Color(0.0, 0.0, 0.0)
    );

    render_with_preview(cam, std::move(world), "out/output.png");
}


int main() {
    switch (7) {
        case 1: simple_light();               break;
        case 2: cornell_box();                break;
        case 5: cornell_box2();               break;
        case 3: cornell_smoke();              break;
        case 4: final_scene(512, 500, 50);    break;
        case 6: cornell_box3();               break;
        case 7: cornell_box4();               break;
    }

    return 0;
}