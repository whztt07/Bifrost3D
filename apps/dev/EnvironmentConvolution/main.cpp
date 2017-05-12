// Convolute environment maps with a GGX distribution.
// ---------------------------------------------------------------------------
// Copyright (C) 2016, Cogwheel. See AUTHORS.txt for authors
//
// This program is open source and distributed under the New BSD License. See
// LICENSE.txt for more detail.
// ---------------------------------------------------------------------------

#include <Cogwheel/Assets/InfiniteAreaLight.h>
#include <Cogwheel/Core/Engine.h>
#include <Cogwheel/Core/Window.h>
#include <Cogwheel/Input/Keyboard.h>
#include <Cogwheel/Math/Quaternion.h>
#include <Cogwheel/Math/RNG.h>

#include <GLFWDriver.h>
#include <StbImageLoader/StbImageLoader.h>
#include <StbImageWriter/StbImageWriter.h>
#include <TinyExr/TinyExr.h>

#include <omp.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#undef RGB

#include <atomic>
#include <array>
#include <fstream>

using namespace Cogwheel::Assets;
using namespace Cogwheel::Core;
using namespace Cogwheel::Input;
using namespace Cogwheel::Math;

//==============================================================================
// GGX distribution.
//==============================================================================
namespace GGX {

struct Sample {
    Vector3f direction;
    float PDF;
};

inline float D(float alpha, float abs_cos_theta) {
    float alpha_sqrd = alpha * alpha;
    float cos_theta_sqrd = abs_cos_theta * abs_cos_theta;
    float tan_theta_sqrd = fmaxf(1.0f - cos_theta_sqrd, 0.0f) / cos_theta_sqrd;
    float cos_theta_cubed = cos_theta_sqrd * cos_theta_sqrd;
    float foo = alpha_sqrd + tan_theta_sqrd; // No idea what to call this.
    return alpha_sqrd / (PI<float>() * cos_theta_cubed * foo * foo);
}

inline float PDF(float alpha, float abs_cos_theta) {
    return D(alpha, abs_cos_theta) * abs_cos_theta;
}

inline Sample sample(float alpha, Vector2f random_sample) {
    float phi = random_sample.y * (2.0f * PI<float>());

    float tan_theta_sqrd = alpha * alpha * random_sample.x / (1.0f - random_sample.x);
    float cos_theta = 1.0f / sqrt(1.0f + tan_theta_sqrd);

    float r = sqrt(fmaxf(1.0f - cos_theta * cos_theta, 0.0f));

    Sample res;
    res.direction = Vector3f(cos(phi) * r, sin(phi) * r, cos_theta);
    res.PDF = PDF(alpha, cos_theta); // We have to be able to inline this to reuse some temporaries.
    return res;
}

} // NS GGX

// Computes the power heuristic of pdf1 and pdf2.
// It is assumed that pdf1 is always valid, i.e. not NaN.
// pdf2 is allowed to be NaN, but generally try to avoid it. :)
inline float power_heuristic(float pdf1, float pdf2) {
    pdf1 *= pdf1;
    pdf2 *= pdf2;
    float result = pdf1 / (pdf1 + pdf2);
    // This is where floating point math gets tricky!
    // If the mis weight is NaN then it can be caused by three things.
    // 1. pdf1 is so insanely high that pdf1 * pdf1 = infinity. In that case we end up with inf / (inf + pdf2^2) and return 1, unless pdf2 was larger than pdf1, i.e. 'more infinite :p', then we return 0.
    // 2. Conversely pdf2 can also be so insanely high that pdf2 * pdf2 = infinity. This is handled analogously to above.
    // 3. pdf2 can also be NaN. In this case the power heuristic is ill-defined and we return 0.
    return !isnan(result) ? result : (pdf1 > pdf2 ? 1.0f : 0.0f);
}

enum class SampleMethod {
    MIS, Light, BSDF
};

void output_convoluted_image(std::string original_image_file, Image image, float roughness) {
    size_t dot_pos = original_image_file.find_last_of('.');
    std::string file_sans_extension = std::string(original_image_file, 0, dot_pos);
    std::string extension = std::string(original_image_file, dot_pos);
    std::ostringstream output_file;
    output_file << file_sans_extension << "_roughness_" << roughness << extension;
    if (extension.compare(".exr") == 0)
        TinyExr::store(image.get_ID(), output_file.str());
    else
        StbImageWriter::write(image, output_file.str());
}

struct Options {
    SampleMethod sample_method;
    int sample_count;

    static Options parse(int argc, char** argv) {
        Options options = { SampleMethod::BSDF, 256 };

        // Skip the first two arguments, the application name and image path.
        for (int argument = 2; argument < argc; ++argument) {
            if (strcmp(argv[argument], "--mis-sampling") == 0 || strcmp(argv[argument], "-m") == 0)
                options.sample_method = SampleMethod::MIS;
            else if (strcmp(argv[argument], "--light-sampling") == 0 || strcmp(argv[argument], "-l") == 0)
                options.sample_method = SampleMethod::Light;
            else if (strcmp(argv[argument], "--bsdf-sampling") == 0 || strcmp(argv[argument], "-b") == 0)
                options.sample_method = SampleMethod::BSDF;
            else if (strcmp(argv[argument], "--sample-count") == 0 || strcmp(argv[argument], "-s") == 0)
                options.sample_count = atoi(argv[++argument]);
        }

        return options;
    }

    std::string to_string() const {
        std::ostringstream out;
        switch (sample_method) {
        case SampleMethod::MIS: out << "MIS sampling, "; break;
        case SampleMethod::Light: out << "Light sampling, "; break;
        case SampleMethod::BSDF: out << "BSDF sampling, "; break;
        }
        out << sample_count << " samples pr pixel.";
        return out.str();
    }
};

std::string g_image_file;
Options g_options;
std::array<Image, 11> g_convoluted_images;

void update(const Engine& engine, void* none) {
    // Initialize render texture
    static GLuint tex_ID = 0u;
    if (tex_ID == 0u) {
        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &tex_ID);
        glBindTexture(GL_TEXTURE_2D, tex_ID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    const Keyboard* const keyboard = engine.get_keyboard();
    if (keyboard->was_released(Keyboard::Key::P))
        for (int i = 0; i < g_convoluted_images.size(); ++i)
            output_convoluted_image(g_image_file, g_convoluted_images[i], i / (g_convoluted_images.size() - 1.0f));

    static int image_index = 0;
    static int uploaded_image_index = -1;
    if (keyboard->was_released(Keyboard::Key::Left))
        --image_index;
    if (keyboard->was_released(Keyboard::Key::Right))
        ++image_index;
    image_index = clamp(image_index, 0, int(g_convoluted_images.size() - 1));

    { // Update the backbuffer.
        const Window& window = engine.get_window();
        glViewport(0, 0, window.get_width(), window.get_height());

        { // Setup matrices. I really don't need to do this every frame, since they never change.
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(-1, 1, -1.f, 1.f, 1.f, -1.f);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
        }

        glBindTexture(GL_TEXTURE_2D, tex_ID);
        if (uploaded_image_index != image_index) {
            const GLint BASE_IMAGE_LEVEL = 0;
            const GLint NO_BORDER = 0;
            Image image = g_convoluted_images[image_index];
            glTexImage2D(GL_TEXTURE_2D, BASE_IMAGE_LEVEL, GL_RGB, image.get_width(), image.get_height(), NO_BORDER, GL_RGB, GL_FLOAT, image.get_pixels());
            uploaded_image_index = image_index;
        }

        glClear(GL_COLOR_BUFFER_BIT);
        glBegin(GL_QUADS); {

            glTexCoord2f(0.0f, 0.0f);
            glVertex3f(-1.0f, -1.0f, 0.f);

            glTexCoord2f(1.0f, 0.0f);
            glVertex3f(1.0f, -1.0f, 0.f);

            glTexCoord2f(1.0f, 1.0f);
            glVertex3f(1.0f, 1.0f, 0.f);

            glTexCoord2f(0.0f, 1.0f);
            glVertex3f(-1.0f, 1.0f, 0.f);

        } glEnd();
    }
}

void initialize(Engine& engine) {
    engine.get_window().set_name("Environment convolution");

    Images::allocate(1);
    Textures::allocate(1);

    std::string file_extension = std::string(g_image_file, g_image_file.length() - 4);
    Image image;
    if (file_extension.compare(".exr") == 0)
        image = TinyExr::load(g_image_file);
    else
        image = StbImageLoader::load(std::string(g_image_file));

    if (!image.exists()) {
        printf("Could not load image: %s\n", g_image_file.c_str());
        exit(1);
    }

    Textures::UID texture_ID = Textures::create2D(image.get_ID(), MagnificationFilter::Linear, MinificationFilter::Linear, WrapMode::Repeat, WrapMode::Clamp);
    InfiniteAreaLight* infinite_area_light = nullptr;
    std::vector<LightSample> light_samples = std::vector<LightSample>();
    if (g_options.sample_method != SampleMethod::BSDF) {
        infinite_area_light = new InfiniteAreaLight(texture_ID);
        light_samples.resize(g_options.sample_count * 8);
        for (int s = 0; s < light_samples.size(); ++s)
            light_samples[s] = infinite_area_light->sample(RNG::sample02(s));
    }

    std::atomic_int finished_pixel_count;
    finished_pixel_count.store(0);
    for (int r = 0; r < g_convoluted_images.size(); ++r) {
        g_convoluted_images[r] = Images::create2D("Convoluted image", PixelFormat::RGB_Float, 1.0f, Vector2ui(image.get_width(), image.get_height()));
        float roughness = r / (g_convoluted_images.size() - 1.0f);
        float alpha = fmaxf(0.00000000001f, roughness * roughness * roughness);

        std::vector<GGX::Sample> ggx_samples = std::vector<GGX::Sample>(g_options.sample_count);
        ggx_samples.resize(g_options.sample_count);
        #pragma omp parallel for schedule(dynamic, 16)
        for (int s = 0; s < ggx_samples.size(); ++s)
            ggx_samples[s] = GGX::sample(alpha, RNG::sample02(s));

        #pragma omp parallel for schedule(dynamic, 16)
        for (int i = 0; i < int(image.get_pixel_count()); ++i) {

            int x = i % image.get_width();
            int y = i / image.get_width();

            Vector2f up_uv = Vector2f((x + 0.5f) / image.get_width(), (y + 0.5f) / image.get_height());
            Vector3f up_vector = latlong_texcoord_to_direction(up_uv);
            Quaternionf up_rotation = Quaternionf::look_in(up_vector);

            RGB radiance = RGB::black();

            switch (g_options.sample_method) {
            case SampleMethod::MIS: {
                int bsdf_sample_count = g_options.sample_count / 2;
                int light_sample_count = g_options.sample_count - bsdf_sample_count;

                for (int s = 0; s < light_sample_count; ++s) {
                    LightSample sample = light_samples[(s + RNG::hash(i)) % light_samples.size()];
                    if (sample.PDF < 0.000000001f)
                        continue;

                    Vector3f local_direction = normalize(inverse_unit(up_rotation) * sample.direction_to_light);
                    float ggx_f = GGX::D(alpha, local_direction.z);
                    if (isnan(ggx_f))
                        continue;

                    float cos_theta = fmaxf(local_direction.z, 0.0f);
                    float mis_weight = power_heuristic(sample.PDF, GGX::PDF(alpha, local_direction.z));
                    radiance += sample.radiance * (mis_weight * ggx_f * cos_theta / sample.PDF);
                }

                for (int s = 0; s < bsdf_sample_count; ++s) {
                    GGX::Sample sample = GGX::sample(alpha, RNG::sample02(s));
                    if (sample.PDF < 0.000000001f)
                        continue;

                    sample.direction = normalize(up_rotation * sample.direction);
                    float mis_weight = power_heuristic(sample.PDF, infinite_area_light->PDF(sample.direction));
                    radiance += infinite_area_light->evaluate(sample.direction) * mis_weight;
                }

                // Account for the samples being split evenly between BSDF and light.
                radiance *= 2.0f;

                break;
            }
            case SampleMethod::Light:
                for (int s = 0; s < g_options.sample_count; ++s) {
                    LightSample sample = light_samples[(s + RNG::hash(i)) % light_samples.size()];
                    if (sample.PDF < 0.000000001f)
                        continue;

                    Vector3f local_direction = inverse_unit(up_rotation) * sample.direction_to_light;
                    float ggx_f = GGX::D(alpha, local_direction.z);
                    if (isnan(ggx_f))
                        continue;

                    float cos_theta = fmaxf(local_direction.z, 0.0f);
                    radiance += sample.radiance * ggx_f * cos_theta / sample.PDF;
                }
                break;
            case SampleMethod::BSDF:
                for (int s = 0; s < g_options.sample_count; ++s) {
                    const GGX::Sample& sample = ggx_samples[s];
                    Vector2f sample_uv = direction_to_latlong_texcoord(up_rotation * sample.direction);
                    radiance += sample2D(texture_ID, sample_uv).rgb();
                }
                break;
            }

            radiance /= float(g_options.sample_count);

            g_convoluted_images[r].set_pixel(RGBA(radiance), Vector2ui(x, y)); // TODO map the underlying array instead.

            ++finished_pixel_count;
            if (omp_get_thread_num() == 0)
                printf("\rProgress: %.2f%%", 100.0f * float(finished_pixel_count) / (image.get_pixel_count() * 11.0f));
        }

        // TOODO only output images if headless.
        output_convoluted_image(g_image_file, g_convoluted_images[r], roughness);
    }

    printf("\rProgress: 100.00%%\n");

    // Hook up update callback.
    engine.add_non_mutating_callback(update, nullptr);
}

void print_usage() {
    char* usage =
        "usage EnvironmentConvolution <path/to/environment.ext>:\n"
        "  -h | --help: Show command line usage for EnvironmentConvolution.\n"
        "  -s | --sample-count. The number of samples pr pixel.\n"
        "  -m | --mis-sampling. Combine light and bsdf samples by multiple importance sampling.\n"
        "  -l | --light-sampling. Draw samples from the environment.\n"
        "  -b | --bsdf-sampling. Draw samples from the GGX distribution.\n";
    printf("%s", usage);
}

int main(int argc, char** argv) {
    printf("Environment convolution\n");

    if (argc == 1 || std::string(argv[1]).compare("-h") == 0 || std::string(argv[1]).compare("--help") == 0) {
        print_usage();
        return 0;
    }

    g_image_file = std::string(argv[1]);

    std::string file_extension = std::string(g_image_file, g_image_file.length() - 4);
    // Check if the file format is supported.
    if (!(file_extension.compare(".bmp") == 0 ||
        file_extension.compare(".exr") == 0 ||
        file_extension.compare(".hdr") == 0 ||
        file_extension.compare(".png") == 0 ||
        file_extension.compare(".tga") == 0)) {
        printf("Unsupported file format: %s\nSupported formats are: bmp, exr, hdr, png and tga.\n", file_extension.c_str());
        return 2;
    }

    g_options = Options::parse(argc, argv);

    printf("Convolute '%s'\n", argv[1]);
    printf("  %s\n", g_options.to_string().c_str());

    GLFWDriver::run(initialize, nullptr);
}