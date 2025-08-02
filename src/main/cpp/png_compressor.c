#include <zlib.h>     
#include <png.h>
#include <turbojpeg.h>
#include "include/png_compressor.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} PngBuffer;

// Callback for libpng to write PNG data into memory
static void png_write_to_memory(png_structp png_ptr, png_bytep data, png_size_t length) {
    PngBuffer* buffer = (PngBuffer*)png_get_io_ptr(png_ptr);
    if (buffer->size + length > buffer->capacity) {
        size_t new_capacity = (buffer->size + length) * 2;
        uint8_t* new_data = (uint8_t*)realloc(buffer->data, new_capacity);
        if (!new_data) {
            png_error(png_ptr, "Out of memory");
            return;
        }
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }
    memcpy(buffer->data + buffer->size, data, length);
    buffer->size += length;
}

static void png_flush(png_structp png_ptr) {
    // No-op for memory writes
}

PngResult compress_to_png(uint8_t* raw_data, int width, int height, int channels) {
    PngResult result = {0, 0};

    // Validate input parameters
    if (!raw_data || width <= 0 || height <= 0 || (channels != 3 && channels != 4)) {
        return result;
    }

    // Calculate expected data size
    size_t expected_size = (size_t)width * (size_t)height * (size_t)channels;
    if (expected_size == 0 || expected_size > SIZE_MAX / 2) {
        return result; // Overflow or invalid size
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return result;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        return result;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        if (result.data) free(result.data);
        result.data = NULL;
        result.length = 0;
        return result;
    }

    PngBuffer buffer = {malloc(1024), 0, 1024};
    if (!buffer.data) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return result;
    }

    png_set_write_fn(png_ptr, &buffer, png_write_to_memory, png_flush);

    int color_type = (channels == 3) ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA;

    png_set_IHDR(png_ptr, info_ptr, width, height, 8,
                 color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    // Optimize compression settings for maximum compression
    png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
    png_set_compression_mem_level(png_ptr, 8);
    png_set_compression_strategy(png_ptr, Z_FILTERED);
    png_set_compression_window_bits(png_ptr, 15);
    
    // Use adaptive filtering for better compression
    png_set_filter(png_ptr, PNG_FILTER_TYPE_BASE, PNG_FILTER_OPTIMIZED);
    
    // Set maximum compression for zlib
    png_set_compression_buffer_size(png_ptr, 8192);

    png_write_info(png_ptr, info_ptr);

    // Allocate row pointers array
    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        free(buffer.data);
        return result;
    }

    // Set up row pointers with bounds checking
    size_t row_stride = (size_t)width * (size_t)channels;
    for (int y = 0; y < height; y++) {
        size_t offset = y * row_stride;
        // Ensure we don't go beyond the expected data bounds
        if (offset + row_stride <= expected_size) {
            row_pointers[y] = raw_data + offset;
        } else {
            // If bounds check fails, use a safe fallback
            row_pointers[y] = raw_data;
        }
    }

    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);

    free(row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    result.data = buffer.data;
    result.length = buffer.size;

    return result;
}

JpegResult compress_to_jpeg(uint8_t* raw_data, int width, int height, int quality) {
    JpegResult result = {0, 0};

    // Validate input parameters
    if (!raw_data || width <= 0 || height <= 0 || quality < 1 || quality > 100) {
        return result;
    }

    // Create TurboJPEG compressor
    tjhandle handle = tjInitCompress();
    if (!handle) {
        return result;
    }

    // Calculate buffer size for compressed JPEG
    unsigned long jpeg_size = 0;
    int success = tjCompress2(handle, raw_data, width, 0, height, 
                             TJPF_RGB, &result.data, &jpeg_size, 
                             TJSAMP_420, quality, TJFLAG_FASTDCT);
    
    if (success == 0 && result.data) {
        result.length = jpeg_size;
    } else {
        if (result.data) {
            tjFree(result.data);
            result.data = NULL;
        }
        result.length = 0;
    }

    tjDestroy(handle);
    return result;
}

void free_png_buffer(uint8_t* buffer) {
    if (buffer) {
        free(buffer);
    }
}

void free_jpeg_buffer(uint8_t* buffer) {
    if (buffer) {
        tjFree(buffer);
    }
}
