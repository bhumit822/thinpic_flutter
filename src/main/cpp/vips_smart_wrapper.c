// vips_smart_wrapper.c
#include <vips/vips.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LOW_MAX_KB 800
#define HIGH_MAX_KB 2000

int init_vips() {
    return vips_init("smart_compressor");
}

static size_t get_file_size_kb(const char* filename) {
    struct stat st;
    if (stat(filename, &st) != 0) return 0;
    return st.st_size / 1024;
}

int smart_compress_image(const char* input_path, const char* output_path) {
    const int high_quality_start = 93;
    const int low_quality_start = 85;
    const int min_quality = 40;
    const int quality_step = 3;

    const int max_kb = strstr(output_path, "compressed") ? LOW_MAX_KB : HIGH_MAX_KB;
    const int up_kb = max_kb * 1.2;
    const int down_kb = max_kb * 0.8;

    for (int quality = (max_kb == HIGH_MAX_KB ? high_quality_start : low_quality_start);
         quality >= min_quality; quality -= quality_step) {

        VipsImage *image = vips_image_new_from_file(input_path, "access", VIPS_ACCESS_SEQUENTIAL, "autorotate", TRUE, NULL);
        if (!image) return 1;

        VipsImage *resized = NULL;

        // Resize only for high quality (mimics 1.3 upscale in Python)
        if (max_kb == HIGH_MAX_KB) {
            if (vips_resize(image, &resized, 1.3, "kernel", VIPS_KERNEL_LANCZOS3, NULL)) {
                g_object_unref(image);
                return 2;
            }
        } else {
            resized = image;
            g_object_ref(resized);
        }

        int result = vips_jpegsave(resized, output_path,
                                   "Q", quality,
                                   "optimize_coding", TRUE,
                                   "strip", TRUE,
                                   NULL);

        g_object_unref(image);
        g_object_unref(resized);

        if (result != 0) return 3;

        size_t size_kb = get_file_size_kb(output_path);
        if (size_kb >= down_kb && size_kb <= up_kb) {
            return 0;  // Success
        }
    }

    return 4;  // No quality met target
}
