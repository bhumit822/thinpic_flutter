#ifndef IMAGE_COMPRESSOR_H
#define IMAGE_COMPRESSOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Supported image formats
typedef enum {
    FORMAT_JPEG = 0,
    FORMAT_PNG = 1,
    FORMAT_WEBP = 2,
    FORMAT_TIFF = 3,
    FORMAT_HEIF = 4,
    FORMAT_JP2K = 5,  // JPEG 2000
    FORMAT_JXL = 6,   // JPEG XL
    FORMAT_GIF = 7,
    FORMAT_AUTO = 8   // Auto-detect based on input file extension
} ImageFormat;

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

// Main compression functions with format support
CompressedImageResult compress_image(const char* input_path, int quality);
CompressedImageResult compress_image_with_format(const char* input_path, int quality, ImageFormat format);
// New function with optional width and height parameters
// If both width and height are provided, the smallest dimension will be used to maintain aspect ratio
// If only one is provided, the other will be calculated to maintain aspect ratio
// If both are 0 or negative, no resizing will be applied
CompressedImageResult compress_image_with_size(const char* input_path, int quality, int target_width, int target_height);
CompressedImageResult compress_image_with_size_and_format(const char* input_path, int quality, int target_width, int target_height, ImageFormat format);
CompressedImageResult compress_large_image(const char* input_path, int quality);
CompressedImageResult compress_large_image_with_format(const char* input_path, int quality, ImageFormat format);
CompressedImageResult compress_large_dslr_image(const char* input_path, int quality);
CompressedImageResult compress_large_dslr_image_with_format(const char* input_path, int quality, ImageFormat format);
CompressedImageResult smart_compress_image(const char* input_path, int target_kb, int type);
CompressedImageResult smart_compress_image_with_format(const char* input_path, int target_kb, int type, ImageFormat format);

// Image info
ImageInfo get_image_info(const char* input_path);

// Utility
void free_compressed_buffer(uint8_t* buffer);
void shutdown_vips(void);
int test_vips_basic(void);

// Helper function to detect format from file extension
ImageFormat detect_format_from_path(const char* input_path);

// Auto-compress function that tries multiple formats to find the smallest file
CompressedImageResult auto_compress_image(const char* input_path, int quality);

// Fast WebP compression for speed-critical applications
CompressedImageResult fast_webp_compress(const char* input_path, int quality);

#ifdef __cplusplus
}
#endif

#endif // IMAGE_COMPRESSOR_H
