#ifndef IMAGE_COMPRESSOR_H
#define IMAGE_COMPRESSOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t* data;
    size_t length;
    int success;
} CompressedImageResult;

typedef struct {
    int width;
    int height;
    int bands;
    int orientation;
    int needs_resize;
    int new_width;
    int new_height;
} ImageInfo;

// Main compression functions
CompressedImageResult compress_image(const char* input_path, int quality);
// New function with optional width and height parameters
// If both width and height are provided, the smallest dimension will be used to maintain aspect ratio
// If only one is provided, the other will be calculated to maintain aspect ratio
// If both are 0 or negative, no resizing will be applied
CompressedImageResult compress_image_with_size(const char* input_path, int quality, int target_width, int target_height);
CompressedImageResult compress_large_image(const char* input_path, int quality);
CompressedImageResult compress_large_dslr_image(const char* input_path, int quality);
CompressedImageResult smart_compress_image(const char* input_path, int target_kb, int type);

// Image info
ImageInfo get_image_info(const char* input_path);

// Utility
void free_compressed_buffer(uint8_t* buffer);
void shutdown_vips(void);
int test_vips_basic(void);

#ifdef __cplusplus
}
#endif

#endif // IMAGE_COMPRESSOR_H
