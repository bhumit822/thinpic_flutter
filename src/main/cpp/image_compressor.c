
#include <vips/vips.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

// Structure to hold compressed image data
typedef struct {
    uint8_t* data;
    size_t length;
    int success;
} CompressedImageResult;

// Structure to hold image information
typedef struct {
    int width;
    int height;
    int bands;
    int orientation;
    int needs_resize;
    int new_width;
    int new_height;
} ImageInfo;

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

// Global flag to track VIPS initialization with mutex protection
static int vips_initialized = 0;
static pthread_mutex_t vips_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
CompressedImageResult compress_large_dslr_image(const char* input_path, int quality);
CompressedImageResult compress_large_image(const char* input_path, int quality);
CompressedImageResult smart_compress_image(const char* input_path, int target_kb, int type);

// Helper function to detect format from file extension
ImageFormat detect_format_from_path(const char* input_path) {
    if (!input_path) return FORMAT_JPEG;
    
    const char* ext = strrchr(input_path, '.');
    if (!ext) return FORMAT_JPEG;
    
    ext++; // Skip the dot
    
    // Convert to lowercase for comparison
    char lower_ext[10];
    int i = 0;
    while (ext[i] && i < 9) {
        lower_ext[i] = (ext[i] >= 'A' && ext[i] <= 'Z') ? ext[i] + 32 : ext[i];
        i++;
    }
    lower_ext[i] = '\0';
    
    if (strcmp(lower_ext, "jpg") == 0 || strcmp(lower_ext, "jpeg") == 0) {
        return FORMAT_JPEG;
    } else if (strcmp(lower_ext, "png") == 0) {
        return FORMAT_PNG;
    } else if (strcmp(lower_ext, "webp") == 0) {
        return FORMAT_WEBP;
    } else if (strcmp(lower_ext, "tiff") == 0 || strcmp(lower_ext, "tif") == 0) {
        return FORMAT_TIFF;
    } else if (strcmp(lower_ext, "heif") == 0 || strcmp(lower_ext, "heic") == 0) {
        return FORMAT_HEIF;
    } else if (strcmp(lower_ext, "jp2") == 0 || strcmp(lower_ext, "j2k") == 0) {
        return FORMAT_JP2K;
    } else if (strcmp(lower_ext, "jxl") == 0) {
        return FORMAT_JXL;
    } else if (strcmp(lower_ext, "gif") == 0) {
        return FORMAT_GIF;
    }
    
    return FORMAT_JPEG; // Default to JPEG
}

// Initialize VIPS if not already initialized (thread-safe)
static int ensure_vips_initialized() {
    pthread_mutex_lock(&vips_mutex);
    
    if (!vips_initialized) {
        if (VIPS_INIT("image_compressor")) {
            printf("[image_compressor] Error: Failed to initialize VIPS\n");
            pthread_mutex_unlock(&vips_mutex);
            return 0;
        }
        vips_initialized = 1;
        printf("[image_compressor] VIPS initialized\n");
    }
    
    pthread_mutex_unlock(&vips_mutex);
    return 1;
}

// Thread-safe image compression function optimized for DSLR images
CompressedImageResult compress_image(const char* input_path, int quality) {
    CompressedImageResult result = {NULL, 0, -1};
    
    // Input validation
    if (!input_path || strlen(input_path) == 0) {
        printf("[image_compressor] Error: Invalid input path\n");
        return result;
    }
    
    if (quality < 1 || quality > 100) {
        printf("[image_compressor] Error: Quality must be between 1 and 100\n");
        return result;
    }
    
    // Check if file exists and get file size
    FILE* file = fopen(input_path, "rb");
    if (!file) {
        printf("[image_compressor] Error: Cannot open file: %s\n", input_path);
        return result;
    }
    
    // Get file size to determine compression strategy
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);
    
    printf("[image_compressor] Compressing image: %s (size: %ld bytes, quality: %d)\n", input_path, file_size, quality);
    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        printf("[image_compressor] Error: VIPS initialization failed\n");
        return result;
    }
    
    // Lock VIPS operations for this thread
    pthread_mutex_lock(&vips_mutex);
    
    // Clear any previous errors
    vips_error_clear();
    
    VipsImage* image = NULL;
    VipsImage* processed_image = NULL;
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    // Load image with error handling
    printf("[image_compressor] Loading image...\n");
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load image\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        vips_error_clear();
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Validate image object
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image object\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get image info
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    int bands = vips_image_get_bands(image);
    
    printf("[image_compressor] Image loaded: %dx%d, %d bands\n", width, height, bands);
    
    // Validate image dimensions
    if (width <= 0 || height <= 0 || bands <= 0) {
        printf("[image_compressor] Error: Invalid image dimensions\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Compression strategy based on dimensions only
    int new_width = width;
    int new_height = height;
    int needs_resize = 0;
    
    // Resize if largest dimension > 6000px
    const int max_dimension = 6000;
    if (width > max_dimension || height > max_dimension) {
        needs_resize = 1;
        if (width > height) {
            new_width = max_dimension;
            new_height = (int)((double)height * max_dimension / width);
        } else {
            new_height = max_dimension;
            new_width = (int)((double)width * max_dimension / height);
        }
        printf("[image_compressor] Resizing from %dx%d to %dx%d\n", width, height, new_width, new_height);
    }
    
    // Process image (resize if needed and convert to sRGB)
    vips_error_clear();
    
    if (needs_resize) {
        printf("[image_compressor] Resizing image with high quality...\n");
        double scale = 1.0;
        if (width > height) {
            scale = (double)max_dimension / width;
        } else {
            scale = (double)max_dimension / height;
        }
        
        printf("[image_compressor] Scale factor: %f\n", scale);
        
        if (vips_resize(image, &processed_image, scale, 
                "kernel", VIPS_KERNEL_LANCZOS3,  // High-quality kernel
                NULL)) {
            printf("[image_compressor] Error: Failed to resize image\n");
            const char* error = vips_error_buffer();
            if (error && strlen(error) > 0) {
                printf("[image_compressor] VIPS error: %s\n", error);
            }
            vips_error_clear();
            
            // Try to compress original image without resizing
            printf("[image_compressor] Trying to compress original image without resizing...\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        g_object_unref(image);
        image = processed_image;
        processed_image = NULL;
        
        // Get new dimensions
        width = vips_image_get_width(image);
        height = vips_image_get_height(image);
        printf("[image_compressor] Image resized to: %dx%d\n", width, height);
        
        // Validate resized image
        if (width <= 0 || height <= 0) {
            printf("[image_compressor] Error: Invalid dimensions after resize\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
    }
    
    // Convert to sRGB for consistent color space
    printf("[image_compressor] Converting image to sRGB...\n");
    vips_error_clear();
    if (vips_copy(image, &processed_image, 
            "interpretation", VIPS_INTERPRETATION_sRGB,
            NULL)) {
        printf("[image_compressor] Error: Failed to convert image to sRGB\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        vips_error_clear();
        
        // Try to compress without sRGB conversion
        printf("[image_compressor] Trying to compress image without sRGB conversion...\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    g_object_unref(image);
    image = processed_image;
    processed_image = NULL;
    
    // Validate final image before compression
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image after processing\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    int final_width = vips_image_get_width(image);
    int final_height = vips_image_get_height(image);
    int final_bands = vips_image_get_bands(image);
    printf("[image_compressor] Final image: %dx%d, %d bands\n", final_width, final_height, final_bands);
    
    // Compression with user-specified quality
    printf("[image_compressor] Starting compression...\n");
    vips_error_clear();
    
    // Use user-specified quality
    int final_quality = quality;
    printf("[image_compressor] Using quality: %d\n", final_quality);
    
    // Enhanced JPEG save options for better compression
    int save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
        "Q", final_quality,
        // "strip", TRUE,
        "optimize_coding", TRUE,
        "interlace", FALSE,  // Better for most images
        "no_subsample", FALSE, // Allow subsampling for better compression
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        // Compression successful
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Compression successful: %zu bytes (quality: %d)\n", buffer_size, final_quality);
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // If enhanced compression failed, try standard approach
    printf("[image_compressor] Enhanced compression failed, trying standard approach...\n");
    const char* error = vips_error_buffer();
    if (error && strlen(error) > 0) {
        printf("[image_compressor] VIPS error: %s\n", error);
    }
    vips_error_clear();
    
    // Free any existing buffer
    if (buffer) {
        g_free(buffer);
        buffer = NULL;
        buffer_size = 0;
    }
    
    // Try with standard quality
    save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
        "Q", quality,
        // "strip", TRUE,
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Standard compression successful: %zu bytes\n", buffer_size);
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // All compression attempts failed
    printf("[image_compressor] Error: All compression attempts failed\n");
    error = vips_error_buffer();
    if (error && strlen(error) > 0) {
        printf("[image_compressor] VIPS error: %s\n", error);
    }
    
    // Cleanup
    if (buffer) {
        g_free(buffer);
    }
    g_object_unref(image);
    vips_error_clear();
    
    pthread_mutex_unlock(&vips_mutex);
    return result;
}

// Thread-safe image compression function with format support
CompressedImageResult compress_image_with_format(const char* input_path, int quality, ImageFormat format) {
    CompressedImageResult result = {NULL, 0, -1};
    
    // Input validation
    if (!input_path || strlen(input_path) == 0) {
        printf("[image_compressor] Error: Invalid input path\n");
        return result;
    }
    
    if (quality < 1 || quality > 100) {
        printf("[image_compressor] Error: Quality must be between 1 and 100\n");
        return result;
    }
    
    // Auto-detect format if requested
    if (format == FORMAT_AUTO) {
        format = detect_format_from_path(input_path);
        printf("[image_compressor] Auto-detected format: %d\n", format);
    }
    
    // Check if file exists and get file size
    FILE* file = fopen(input_path, "rb");
    if (!file) {
        printf("[image_compressor] Error: Cannot open file: %s\n", input_path);
        return result;
    }
    
    // Get file size to determine compression strategy
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);
    
    printf("[image_compressor] Compressing image: %s (size: %ld bytes, quality: %d, format: %d)\n", 
           input_path, file_size, quality, format);
    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        printf("[image_compressor] Error: VIPS initialization failed\n");
        return result;
    }
    
    // Lock VIPS operations for this thread
    pthread_mutex_lock(&vips_mutex);
    
    // Clear any previous errors
    vips_error_clear();
    
    VipsImage* image = NULL;
    VipsImage* processed_image = NULL;
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    // Load image with error handling
    printf("[image_compressor] Loading image...\n");
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load image\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        vips_error_clear();
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Validate image object
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image object\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get image info
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    int bands = vips_image_get_bands(image);
    
    printf("[image_compressor] Image loaded: %dx%d, %d bands\n", width, height, bands);
    
    // Validate image dimensions
    if (width <= 0 || height <= 0 || bands <= 0) {
        printf("[image_compressor] Error: Invalid image dimensions\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Compression strategy based on dimensions only
    int new_width = width;
    int new_height = height;
    int needs_resize = 0;
    
    // Resize if largest dimension > 6000px
    const int max_dimension = 6000;
    if (width > max_dimension || height > max_dimension) {
        needs_resize = 1;
        if (width > height) {
            new_width = max_dimension;
            new_height = (int)((double)height * max_dimension / width);
        } else {
            new_height = max_dimension;
            new_width = (int)((double)width * max_dimension / height);
        }
        printf("[image_compressor] Resizing from %dx%d to %dx%d\n", width, height, new_width, new_height);
    }
    
    // Process image (resize if needed and convert to sRGB)
    vips_error_clear();
    
    if (needs_resize) {
        printf("[image_compressor] Resizing image with high quality...\n");
        double scale = 1.0;
        if (width > height) {
            scale = (double)max_dimension / width;
        } else {
            scale = (double)max_dimension / height;
        }
        
        printf("[image_compressor] Scale factor: %f\n", scale);
        
        if (vips_resize(image, &processed_image, scale, 
                "kernel", VIPS_KERNEL_LANCZOS3,  // High-quality kernel
                NULL)) {
            printf("[image_compressor] Error: Failed to resize image\n");
            const char* error = vips_error_buffer();
            if (error && strlen(error) > 0) {
                printf("[image_compressor] VIPS error: %s\n", error);
            }
            vips_error_clear();
            
            // Try to compress original image without resizing
            printf("[image_compressor] Trying to compress original image without resizing...\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        g_object_unref(image);
        image = processed_image;
        processed_image = NULL;
        
        // Get new dimensions
        width = vips_image_get_width(image);
        height = vips_image_get_height(image);
        printf("[image_compressor] Image resized to: %dx%d\n", width, height);
        
        // Validate resized image
        if (width <= 0 || height <= 0) {
            printf("[image_compressor] Error: Invalid dimensions after resize\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
    }
    
    // Convert to sRGB for consistent color space (except for GIF which should remain as-is)
    if (format != FORMAT_GIF) {
        printf("[image_compressor] Converting image to sRGB...\n");
        vips_error_clear();
        if (vips_copy(image, &processed_image, 
                "interpretation", VIPS_INTERPRETATION_sRGB,
                NULL)) {
            printf("[image_compressor] Error: Failed to convert image to sRGB\n");
            const char* error = vips_error_buffer();
            if (error && strlen(error) > 0) {
                printf("[image_compressor] VIPS error: %s\n", error);
            }
            vips_error_clear();
            
            // Try to compress without sRGB conversion
            printf("[image_compressor] Trying to compress image without sRGB conversion...\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        
        g_object_unref(image);
        image = processed_image;
        processed_image = NULL;
    }
    
    // Validate final image before compression
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image after processing\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    int final_width = vips_image_get_width(image);
    int final_height = vips_image_get_height(image);
    int final_bands = vips_image_get_bands(image);
    printf("[image_compressor] Final image: %dx%d, %d bands\n", final_width, final_height, final_bands);
    
    // Compression with format-specific settings
    printf("[image_compressor] Starting compression with format %d...\n", format);
    vips_error_clear();
    
    int save_result = -1;
    
    switch (format) {
        case FORMAT_JPEG:
            save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "optimize_coding", TRUE,
                "interlace", FALSE,
                "no_subsample", FALSE,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_PNG:
            // PNG quality is 0-9, convert from 1-100
            // For better compression, use higher compression levels for lower quality
            int png_quality = 9 - ((quality * 9) / 100);
            if (png_quality < 0) png_quality = 0;
            if (png_quality > 9) png_quality = 9;
            
            save_result = vips_pngsave_buffer(image, &buffer, &buffer_size,
                "compression", png_quality,
                "interlace", FALSE,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_WEBP:
            save_result = vips_webpsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                "near_lossless", FALSE,
                "smart_subsample", FALSE,  // Disable for faster compression
                "strip", FALSE,  // Keep orientation data
                "effort", 2,     // Lower effort for faster compression (0-6, default is 4)
                NULL);
            break;
            
        case FORMAT_TIFF:
            save_result = vips_tiffsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "compression", VIPS_FOREIGN_TIFF_COMPRESSION_JPEG,
                "predictor", VIPS_FOREIGN_TIFF_PREDICTOR_HORIZONTAL,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_HEIF:
            save_result = vips_heifsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_JP2K:
            save_result = vips_jp2ksave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_JXL:
            save_result = vips_jxlsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_GIF:
            // GIF doesn't support quality, use default settings
            save_result = vips_gifsave_buffer(image, &buffer, &buffer_size,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        default:
            printf("[image_compressor] Error: Unsupported format %d\n", format);
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
    }
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        // Compression successful
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Compression successful: %zu bytes (format: %d, quality: %d)\n", 
               buffer_size, format, quality);
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Compression failed
    printf("[image_compressor] Error: Compression failed for format %d\n", format);
    const char* error = vips_error_buffer();
    if (error && strlen(error) > 0) {
        printf("[image_compressor] VIPS error: %s\n", error);
    }
    
    // Cleanup
    if (buffer) {
        g_free(buffer);
    }
    g_object_unref(image);
    vips_error_clear();
    
    pthread_mutex_unlock(&vips_mutex);
    return result;
}

// Thread-safe image compression function with optional size parameters
CompressedImageResult compress_image_with_size(const char* input_path, int quality, int target_width, int target_height) {
    CompressedImageResult result = {NULL, 0, -1};
    
    // Input validation
    if (!input_path || strlen(input_path) == 0) {
        printf("[image_compressor] Error: Invalid input path\n");
        return result;
    }
    
    if (quality < 1 || quality > 100) {
        printf("[image_compressor] Error: Quality must be between 1 and 100\n");
        return result;
    }
    
    // Check if file exists and get file size
    FILE* file = fopen(input_path, "rb");
    if (!file) {
        printf("[image_compressor] Error: Cannot open file: %s\n", input_path);
        return result;
    }
    
    // Get file size to determine compression strategy
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);
    
    printf("[image_compressor] Compressing image: %s (size: %ld bytes, quality: %d, target: %dx%d)\n", 
           input_path, file_size, quality, target_width, target_height);
    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        printf("[image_compressor] Error: VIPS initialization failed\n");
        return result;
    }
    
    // Lock VIPS operations for this thread
    pthread_mutex_lock(&vips_mutex);
    
    // Clear any previous errors
    vips_error_clear();
    
    VipsImage* image = NULL;
    VipsImage* processed_image = NULL;
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    // Load image with error handling
    printf("[image_compressor] Loading image...\n");
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load image\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        vips_error_clear();
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Validate image object
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image object\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get image info
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    int bands = vips_image_get_bands(image);
    
    printf("[image_compressor] Image loaded: %dx%d, %d bands\n", width, height, bands);
    
    // Validate image dimensions
    if (width <= 0 || height <= 0 || bands <= 0) {
        printf("[image_compressor] Error: Invalid image dimensions\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Calculate resize parameters
    int new_width = width;
    int new_height = height;
    int needs_resize = 0;
    double scale = 1.0;
    
    // Check if user provided target dimensions
    if (target_width > 0 || target_height > 0) {
        needs_resize = 1;
        
        if (target_width > 0 && target_height > 0) {
            // Both dimensions provided - use the smallest to maintain aspect ratio
            double scale_width = (double)target_width / width;
            double scale_height = (double)target_height / height;
            
            if (scale_width < scale_height) {
                // Width is the limiting factor
                scale = scale_width;
                new_width = target_width;
                new_height = (int)(height * scale_width);
            } else {
                // Height is the limiting factor
                scale = scale_height;
                new_height = target_height;
                new_width = (int)(width * scale_height);
            }
            printf("[image_compressor] Both dimensions provided, using smallest scale: %f\n", scale);
        } else if (target_width > 0) {
            // Only width provided
            scale = (double)target_width / width;
            new_width = target_width;
            new_height = (int)(height * scale);
            printf("[image_compressor] Only width provided, calculated height: %d\n", new_height);
        } else {
            // Only height provided
            scale = (double)target_height / height;
            new_height = target_height;
            new_width = (int)(width * scale);
            printf("[image_compressor] Only height provided, calculated width: %d\n", new_width);
        }
        
        printf("[image_compressor] Resizing from %dx%d to %dx%d (scale: %f)\n", 
               width, height, new_width, new_height, scale);
    } else {
        // No target dimensions provided - use original logic for large images
        const int max_dimension = 6000;
        if (width > max_dimension || height > max_dimension) {
            needs_resize = 1;
            if (width > height) {
                scale = (double)max_dimension / width;
                new_width = max_dimension;
                new_height = (int)(height * scale);
            } else {
                scale = (double)max_dimension / height;
                new_height = max_dimension;
                new_width = (int)(width * scale);
            }
            printf("[image_compressor] Large image auto-resize from %dx%d to %dx%d\n", 
                   width, height, new_width, new_height);
        }
    }
    
    // Process image (resize if needed and convert to sRGB)
    vips_error_clear();
    
    if (needs_resize) {
        printf("[image_compressor] Resizing image with high quality...\n");
        
        if (vips_resize(image, &processed_image, scale, 
                "kernel", VIPS_KERNEL_LANCZOS3,  // High-quality kernel
                NULL)) {
            printf("[image_compressor] Error: Failed to resize image\n");
            const char* error = vips_error_buffer();
            if (error && strlen(error) > 0) {
                printf("[image_compressor] VIPS error: %s\n", error);
            }
            vips_error_clear();
            
            // Try to compress original image without resizing
            printf("[image_compressor] Trying to compress original image without resizing...\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        g_object_unref(image);
        image = processed_image;
        processed_image = NULL;
        
        // Get new dimensions
        width = vips_image_get_width(image);
        height = vips_image_get_height(image);
        printf("[image_compressor] Image resized to: %dx%d\n", width, height);
        
        // Validate resized image
        if (width <= 0 || height <= 0) {
            printf("[image_compressor] Error: Invalid dimensions after resize\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
    }
    
    // Convert to sRGB for consistent color space
    printf("[image_compressor] Converting image to sRGB...\n");
    vips_error_clear();
    if (vips_copy(image, &processed_image, 
            "interpretation", VIPS_INTERPRETATION_sRGB,
            NULL)) {
        printf("[image_compressor] Error: Failed to convert image to sRGB\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        vips_error_clear();
        
        // Try to compress without sRGB conversion
        printf("[image_compressor] Trying to compress image without sRGB conversion...\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    g_object_unref(image);
    image = processed_image;
    processed_image = NULL;
    
    // Validate final image before compression
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image after processing\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    int final_width = vips_image_get_width(image);
    int final_height = vips_image_get_height(image);
    int final_bands = vips_image_get_bands(image);
    printf("[image_compressor] Final image: %dx%d, %d bands\n", final_width, final_height, final_bands);
    
    // Compression with user-specified quality
    printf("[image_compressor] Starting compression...\n");
    vips_error_clear();
    
    // Use user-specified quality
    int final_quality = quality;
    printf("[image_compressor] Using quality: %d\n", final_quality);
    
    // Enhanced JPEG save options for better compression
    int save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
        "Q", final_quality,
        // "strip", TRUE,
        "optimize_coding", TRUE,
        "interlace", FALSE,  // Better for most images
        "no_subsample", FALSE, // Allow subsampling for better compression
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        // Compression successful
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Compression successful: %zu bytes (quality: %d)\n", buffer_size, final_quality);
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // If enhanced compression failed, try standard approach
    printf("[image_compressor] Enhanced compression failed, trying standard approach...\n");
    const char* error = vips_error_buffer();
    if (error && strlen(error) > 0) {
        printf("[image_compressor] VIPS error: %s\n", error);
    }
    vips_error_clear();
    
    // Free any existing buffer
    if (buffer) {
        g_free(buffer);
        buffer = NULL;
        buffer_size = 0;
    }
    
    // Try with standard quality
    save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
        "Q", quality,
        // "strip", TRUE,
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Standard compression successful: %zu bytes\n", buffer_size);
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // All compression attempts failed
    printf("[image_compressor] Error: All compression attempts failed\n");
    error = vips_error_buffer();
    if (error && strlen(error) > 0) {
        printf("[image_compressor] VIPS error: %s\n", error);
    }
    
    // Cleanup
    if (buffer) {
        g_free(buffer);
    }
    g_object_unref(image);
    vips_error_clear();
    
    pthread_mutex_unlock(&vips_mutex);
    return result;
}

// Thread-safe image compression function with size parameters and format support
CompressedImageResult compress_image_with_size_and_format(const char* input_path, int quality, int target_width, int target_height, ImageFormat format) {
    CompressedImageResult result = {NULL, 0, -1};
    
    // Input validation
    if (!input_path || strlen(input_path) == 0) {
        printf("[image_compressor] Error: Invalid input path\n");
        return result;
    }
    
    if (quality < 1 || quality > 100) {
        printf("[image_compressor] Error: Quality must be between 1 and 100\n");
        return result;
    }
    
    // Auto-detect format if requested
    if (format == FORMAT_AUTO) {
        format = detect_format_from_path(input_path);
        printf("[image_compressor] Auto-detected format: %d\n", format);
    }
    
    // Check if file exists and get file size
    FILE* file = fopen(input_path, "rb");
    if (!file) {
        printf("[image_compressor] Error: Cannot open file: %s\n", input_path);
        return result;
    }
    
    // Get file size to determine compression strategy
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);
    
    printf("[image_compressor] Compressing image: %s (size: %ld bytes, quality: %d, target: %dx%d, format: %d)\n", 
           input_path, file_size, quality, target_width, target_height, format);
    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        printf("[image_compressor] Error: VIPS initialization failed\n");
        return result;
    }
    
    // Lock VIPS operations for this thread
    pthread_mutex_lock(&vips_mutex);
    
    // Clear any previous errors
    vips_error_clear();
    
    VipsImage* image = NULL;
    VipsImage* processed_image = NULL;
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    // Load image with error handling
    printf("[image_compressor] Loading image...\n");
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load image\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        vips_error_clear();
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Validate image object
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image object\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get image info
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    int bands = vips_image_get_bands(image);
    
    printf("[image_compressor] Image loaded: %dx%d, %d bands\n", width, height, bands);
    
    // Validate image dimensions
    if (width <= 0 || height <= 0 || bands <= 0) {
        printf("[image_compressor] Error: Invalid image dimensions\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Resize logic based on target dimensions
    int needs_resize = 0;
    int new_width = width;
    int new_height = height;
    
    if (target_width > 0 && target_height > 0) {
        // Both dimensions provided - use the smaller one to maintain aspect ratio
        double scale_x = (double)target_width / width;
        double scale_y = (double)target_height / height;
        double scale = (scale_x < scale_y) ? scale_x : scale_y;
        
        new_width = (int)(width * scale);
        new_height = (int)(height * scale);
        needs_resize = 1;
        
        printf("[image_compressor] Resizing to fit %dx%d: %dx%d -> %dx%d (scale: %f)\n", 
               target_width, target_height, width, height, new_width, new_height, scale);
    } else if (target_width > 0) {
        // Only width provided
        double scale = (double)target_width / width;
        new_width = target_width;
        new_height = (int)(height * scale);
        needs_resize = 1;
        
        printf("[image_compressor] Resizing to width %d: %dx%d -> %dx%d (scale: %f)\n", 
               target_width, width, height, new_width, new_height, scale);
    } else if (target_height > 0) {
        // Only height provided
        double scale = (double)target_height / height;
        new_width = (int)(width * scale);
        new_height = target_height;
        needs_resize = 1;
        
        printf("[image_compressor] Resizing to height %d: %dx%d -> %dx%d (scale: %f)\n", 
               target_height, width, height, new_width, new_height, scale);
    } else {
        // No target dimensions - use default max dimension logic
        const int max_dimension = 6000;
        if (width > max_dimension || height > max_dimension) {
            needs_resize = 1;
            if (width > height) {
                new_width = max_dimension;
                new_height = (int)((double)height * max_dimension / width);
            } else {
                new_height = max_dimension;
                new_width = (int)((double)width * max_dimension / height);
            }
            printf("[image_compressor] Resizing from %dx%d to %dx%d (max dimension: %d)\n", 
                   width, height, new_width, new_height, max_dimension);
        }
    }
    
    // Process image (resize if needed and convert to sRGB)
    vips_error_clear();
    
    if (needs_resize) {
        printf("[image_compressor] Resizing image with high quality...\n");
        double scale = (double)new_width / width;
        
        printf("[image_compressor] Scale factor: %f\n", scale);
        
        if (vips_resize(image, &processed_image, scale, 
                "kernel", VIPS_KERNEL_LANCZOS3,  // High-quality kernel
                NULL)) {
            printf("[image_compressor] Error: Failed to resize image\n");
            const char* error = vips_error_buffer();
            if (error && strlen(error) > 0) {
                printf("[image_compressor] VIPS error: %s\n", error);
            }
            vips_error_clear();
            
            // Try to compress original image without resizing
            printf("[image_compressor] Trying to compress original image without resizing...\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        g_object_unref(image);
        image = processed_image;
        processed_image = NULL;
        
        // Get new dimensions
        width = vips_image_get_width(image);
        height = vips_image_get_height(image);
        printf("[image_compressor] Image resized to: %dx%d\n", width, height);
        
        // Validate resized image
        if (width <= 0 || height <= 0) {
            printf("[image_compressor] Error: Invalid dimensions after resize\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
    }
    
    // Convert to sRGB for consistent color space (except for GIF which should remain as-is)
    if (format != FORMAT_GIF) {
        printf("[image_compressor] Converting image to sRGB...\n");
        vips_error_clear();
        if (vips_copy(image, &processed_image, 
                "interpretation", VIPS_INTERPRETATION_sRGB,
                NULL)) {
            printf("[image_compressor] Error: Failed to convert image to sRGB\n");
            const char* error = vips_error_buffer();
            if (error && strlen(error) > 0) {
                printf("[image_compressor] VIPS error: %s\n", error);
            }
            vips_error_clear();
            
            // Try to compress without sRGB conversion
            printf("[image_compressor] Trying to compress image without sRGB conversion...\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        
        g_object_unref(image);
        image = processed_image;
        processed_image = NULL;
    }
    
    // Validate final image before compression
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image after processing\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    int final_width = vips_image_get_width(image);
    int final_height = vips_image_get_height(image);
    int final_bands = vips_image_get_bands(image);
    printf("[image_compressor] Final image: %dx%d, %d bands\n", final_width, final_height, final_bands);
    
    // Compression with format-specific settings
    printf("[image_compressor] Starting compression with format %d...\n", format);
    vips_error_clear();
    
    int save_result = -1;
    
    switch (format) {
        case FORMAT_JPEG:
            save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "optimize_coding", TRUE,
                "interlace", FALSE,
                "no_subsample", FALSE,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_PNG:
            // PNG quality is 0-9, convert from 1-100
            // For better compression, use higher compression levels for lower quality
            int png_quality = 9 - ((quality * 9) / 100);
            if (png_quality < 0) png_quality = 0;
            if (png_quality > 9) png_quality = 9;
            
            save_result = vips_pngsave_buffer(image, &buffer, &buffer_size,
                "compression", png_quality,
                "interlace", FALSE,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_WEBP:
            save_result = vips_webpsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                "near_lossless", FALSE,
                "smart_subsample", FALSE,  // Disable for faster compression
                "strip", FALSE,  // Keep orientation data
                "effort", 2,     // Lower effort for faster compression (0-6, default is 4)
                NULL);
            break;
            
        case FORMAT_TIFF:
            save_result = vips_tiffsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "compression", VIPS_FOREIGN_TIFF_COMPRESSION_JPEG,
                "predictor", VIPS_FOREIGN_TIFF_PREDICTOR_HORIZONTAL,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_HEIF:
            save_result = vips_heifsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_JP2K:
            save_result = vips_jp2ksave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_JXL:
            save_result = vips_jxlsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                //"strip", FALSE,  // Keep orientation data
                NULL);
            break;
            
        case FORMAT_GIF:
            // Only try GIF if image has multiple bands (might be animated)
            if (final_bands >= 3) {
                save_result = vips_gifsave_buffer(image, &buffer, &buffer_size,
                    //"strip", FALSE,  // Keep orientation data
                    NULL);
            } else {
                save_result = -1; // Skip GIF for non-animated images
            }
            break;
            
        default:
            printf("[image_compressor] Error: Unsupported format %d\n", format);
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
    }
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        // Compression successful
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Compression successful: %zu bytes (format: %d, quality: %d)\n", 
               buffer_size, format, quality);
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Compression failed
    printf("[image_compressor] Error: Compression failed for format %d\n", format);
    const char* error = vips_error_buffer();
    if (error && strlen(error) > 0) {
        printf("[image_compressor] VIPS error: %s\n", error);
    }
    
    // Cleanup
    if (buffer) {
        g_free(buffer);
    }
    g_object_unref(image);
    vips_error_clear();
    
    pthread_mutex_unlock(&vips_mutex);
    return result;
}

// Function to free the compressed buffer (thread-safe)
void free_compressed_buffer(uint8_t* buffer) {
    if (buffer) {
        g_free(buffer);
        printf("[image_compressor] Buffer freed\n");
    }
}

// Function to shutdown VIPS (call this when app is closing)
void shutdown_vips() {
    pthread_mutex_lock(&vips_mutex);
    if (vips_initialized) {
        vips_shutdown();
        vips_initialized = 0;
        printf("[image_compressor] VIPS shutdown\n");
    }
    pthread_mutex_unlock(&vips_mutex);
}

// Simple test function to verify VIPS is working
int test_vips_basic() {
    printf("[image_compressor] Testing basic VIPS functionality...\n");
    
    // Initialize VIPS
    if (!ensure_vips_initialized()) {
        printf("[image_compressor] Test failed: VIPS initialization\n");
        return -1;
    }
    
    pthread_mutex_lock(&vips_mutex);
    
    // Try to create a simple 1x1 image
    VipsImage* test_image = NULL;
    vips_error_clear();
    
    if (vips_black(&test_image, 1, 1, NULL)) {
        printf("[image_compressor] Test failed: Cannot create test image\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        pthread_mutex_unlock(&vips_mutex);
        return -1;
    }
    
    // Try to save it as JPEG
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    if (vips_jpegsave_buffer(test_image, &buffer, &buffer_size, NULL)) {
        printf("[image_compressor] Test failed: Cannot save test image\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        g_object_unref(test_image);
        pthread_mutex_unlock(&vips_mutex);
        return -1;
    }
    
    printf("[image_compressor] Test successful: Created and saved %zu bytes\n", buffer_size);
    
    // Cleanup
    g_free(buffer);
    g_object_unref(test_image);
    pthread_mutex_unlock(&vips_mutex);
    
    return 0;
}

// Function to get image information
ImageInfo get_image_info(const char* input_path) {
    ImageInfo info = {0, 0, 0, 0, 0, 0, 0};
    
    // Input validation
    if (!input_path || strlen(input_path) == 0) {
        printf("[image_compressor] Error: Invalid input path for info\n");
        return info;
    }
    
    // Check if file exists
    FILE* file = fopen(input_path, "rb");
    if (!file) {
        printf("[image_compressor] Error: Cannot open file for info: %s\n", input_path);
        return info;
    }
    fclose(file);
    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        return info;
    }
    
    // Lock VIPS operations for this thread
    pthread_mutex_lock(&vips_mutex);
    
    // Clear any previous errors
    vips_error_clear();
    
    VipsImage* image = NULL;
    
    // Load image with error handling
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load image for info\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        vips_error_clear();
        pthread_mutex_unlock(&vips_mutex);
        return info;
    }
    
    // Validate image object
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image object for info\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return info;
    }
    
    // Get image info
    info.width = vips_image_get_width(image);
    info.height = vips_image_get_height(image);
    info.bands = vips_image_get_bands(image);
    
    // Get orientation from EXIF if available
    VipsImage* orientation_image = NULL;
    if (vips_copy(image, &orientation_image, NULL) == 0) {
        // Try to get orientation from EXIF
        const char* orientation_str = NULL;
        if (vips_image_get_string(orientation_image, "exif-ifd0-Orientation", &orientation_str) == 0) {
            if (orientation_str) {
                info.orientation = atoi(orientation_str);
            }
        }
        g_object_unref(orientation_image);
    }
    
    // Check if image needs resizing (largest side > 6000px)
    const int max_dimension = 6000;
    
    if (info.width > max_dimension || info.height > max_dimension) {
        info.needs_resize = 1;
        if (info.width > info.height) {
            // Landscape image
            info.new_width = max_dimension;
            info.new_height = (int)((double)info.height * max_dimension / info.width);
        } else {
            // Portrait or square image
            info.new_height = max_dimension;
            info.new_width = (int)((double)info.width * max_dimension / info.height);
        }
    }
    
    printf("[image_compressor] Image info: %dx%d, %d bands, orientation: %d, needs_resize: %d\n", 
           info.width, info.height, info.bands, info.orientation, info.needs_resize);
    
    // Cleanup
    g_object_unref(image);
    pthread_mutex_unlock(&vips_mutex);
    
    return info;
}

// Function to handle very large images by creating a smaller version
CompressedImageResult compress_large_image(const char* input_path, int quality) {
    CompressedImageResult result = {NULL, 0, -1};
    
    printf("[image_compressor] Handling very large image: %s\n", input_path);
    

    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        return result;
    }
    
    pthread_mutex_lock(&vips_mutex);
    
    VipsImage* image = NULL;
    VipsImage* small_image = NULL;
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    // Load image
    vips_error_clear();
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load large image\n");
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get dimensions
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    
    printf("[image_compressor] Large image: %dx%d\n", width, height);
    
    // Create a smaller version for compression (max 6000px)
    const int max_dimension = 6000;
    double scale = 1.0;
    if (width > height) {
        scale = (double)max_dimension / width;
    } else {
        scale = (double)max_dimension / height;
    }
    
    printf("[image_compressor] Creating smaller version with scale: %f\n", scale);
    
    if (vips_resize(image, &small_image, scale, NULL)) {
        printf("[image_compressor] Error: Failed to create smaller version\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    g_object_unref(image);
    image = small_image;
    
    // Convert to sRGB
    VipsImage* srgb_image = NULL;
    if (vips_copy(image, &srgb_image, 
            "interpretation", VIPS_INTERPRETATION_sRGB,
            NULL)) {
        printf("[image_compressor] Error: Failed to convert to sRGB\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    g_object_unref(image);
    image = srgb_image;
    
    // Use user-specified quality
    int final_quality = quality;
    printf("[image_compressor] Compressing with quality: %d\n", final_quality);
    
    vips_error_clear();
    int save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
        "Q", final_quality,
        // "strip", TRUE,
        "optimize_coding", TRUE,
        "interlace", FALSE,
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Large image compression successful: %zu bytes\n", buffer_size);
    } else {
        printf("[image_compressor] Error: Failed to compress large image\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
    }
    
    g_object_unref(image);
    pthread_mutex_unlock(&vips_mutex);
    return result;
}

// Function to handle very large DSLR images by creating a smaller version
CompressedImageResult compress_large_dslr_image(const char* input_path, int quality) {
    CompressedImageResult result = {NULL, 0, -1};
    
    printf("[image_compressor] Handling very large DSLR image: %s\n", input_path);
    

    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        return result;
    }
    
    pthread_mutex_lock(&vips_mutex);
    
    VipsImage* image = NULL;
    VipsImage* small_image = NULL;
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    // Load image
    vips_error_clear();
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load large DSLR image\n");
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get dimensions
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    
    printf("[image_compressor] Large DSLR image: %dx%d\n", width, height);
    
    // Create a smaller version for compression (max 6000px)
    const int max_dimension = 6000;
    double scale = 1.0;
    if (width > height) {
        scale = (double)max_dimension / width;
    } else {
        scale = (double)max_dimension / height;
    }
    
    printf("[image_compressor] Creating smaller version with scale: %f\n", scale);
    
    if (vips_resize(image, &small_image, scale, NULL)) {
        printf("[image_compressor] Error: Failed to create smaller version\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    g_object_unref(image);
    image = small_image;
    
    // Convert to sRGB
    VipsImage* srgb_image = NULL;
    if (vips_copy(image, &srgb_image, 
            "interpretation", VIPS_INTERPRETATION_sRGB,
            NULL)) {
        printf("[image_compressor] Error: Failed to convert to sRGB\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    g_object_unref(image);
    image = srgb_image;
    
    // Use user-specified quality
    int final_quality = quality;
    printf("[image_compressor] Compressing DSLR with quality: %d\n", final_quality);
    
    vips_error_clear();
    int save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
        "Q", final_quality,
        // "strip", TRUE,
        "optimize_coding", TRUE,
        "interlace", FALSE,  // Better for DSLR images
        "no_subsample", FALSE, // Allow subsampling for better compression
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Large DSLR image compression successful: %zu bytes\n", buffer_size);
    } else {
        printf("[image_compressor] Error: Failed to compress large DSLR image\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
    }
    
    g_object_unref(image);
    pthread_mutex_unlock(&vips_mutex);
    return result;
}

// Smart compression function that targets a specific file size
CompressedImageResult smart_compress_image(const char* input_path, int target_kb, int type) {
    CompressedImageResult result = {NULL, 0, -1};
    
    printf("[image_compressor] Smart compression: %s (target: %d KB, type: %s)\n", 
           input_path, target_kb, type == 1 ? "high" : "low");
    
    // Input validation
    if (!input_path || strlen(input_path) == 0) {
        printf("[image_compressor] Error: Invalid input path\n");
        return result;
    }
    
    if (target_kb <= 0) {
        printf("[image_compressor] Error: Invalid target KB\n");
        return result;
    }
    
    // Check if file exists
    FILE* file = fopen(input_path, "rb");
    if (!file) {
        printf("[image_compressor] Error: Cannot open file: %s\n", input_path);
        return result;
    }
    fclose(file);
    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        printf("[image_compressor] Error: VIPS initialization failed\n");
        return result;
    }
    
    // Calculate size buffers (20% tolerance)
    int up_size_buffer_kb = (int)(target_kb * 1.2);
    int down_size_buffer_kb = (int)(target_kb * 0.8);
    
    printf("[image_compressor] Target range: %d - %d KB\n", down_size_buffer_kb, up_size_buffer_kb);
    
    // Determine quality range based on type
    int start_quality = (type == 1) ? 93 : 85;  // high = 93, low = 85
    int end_quality = 40;
    int quality_step = 3;
    
    printf("[image_compressor] Quality range: %d to %d (step: %d)\n", start_quality, end_quality, quality_step);
    
    // Lock VIPS operations for this thread
    pthread_mutex_lock(&vips_mutex);
    
    // Try quality sweep from high to low
    for (int quality = start_quality; quality >= end_quality; quality -= quality_step) {
        printf("[image_compressor] Trying quality: %d\n", quality);
        
        // Clear any previous errors
        vips_error_clear();
        
        VipsImage* image = NULL;
        VipsImage* processed_image = NULL;
        void* buffer = NULL;
        size_t buffer_size = 0;
        
        // Load image for this iteration
        image = vips_image_new_from_file(input_path, 
            "fail_on", VIPS_FAIL_ON_NONE,
            "access", VIPS_ACCESS_SEQUENTIAL,

            NULL);
        
        if (!image) {
            printf("[image_compressor] Error: Failed to load image for quality %d\n", quality);
            const char* error = vips_error_buffer();
            if (error && strlen(error) > 0) {
                printf("[image_compressor] VIPS error: %s\n", error);
            }
            vips_error_clear();
            continue; // Try next quality
        }
        
        // Validate image object
        if (!VIPS_IS_IMAGE(image)) {
            printf("[image_compressor] Error: Invalid image object for quality %d\n", quality);
            g_object_unref(image);
            continue; // Try next quality
        }
        
        // Get image dimensions
        int width = vips_image_get_width(image);
        int height = vips_image_get_height(image);
        
        // Apply resize for high quality type
        if (type == 1) { // high quality
            printf("[image_compressor] Applying high quality resize (1.3x)\n");
            if (vips_resize(image, &processed_image, 1.3, 
                    "kernel", VIPS_KERNEL_LANCZOS3,
                    NULL)) {
                printf("[image_compressor] Error: Failed to resize for high quality\n");
                const char* error = vips_error_buffer();
                if (error && strlen(error) > 0) {
                    printf("[image_compressor] VIPS error: %s\n", error);
                }
                vips_error_clear();
                g_object_unref(image);
                continue; // Try next quality
            }
            g_object_unref(image);
            image = processed_image;
            processed_image = NULL;
        }
        
        // Convert to sRGB for consistent color space
        vips_error_clear();
        if (vips_copy(image, &processed_image, 
                "interpretation", VIPS_INTERPRETATION_sRGB,
                NULL)) {
            printf("[image_compressor] Error: Failed to convert to sRGB for quality %d\n", quality);
            const char* error = vips_error_buffer();
            if (error && strlen(error) > 0) {
                printf("[image_compressor] VIPS error: %s\n", error);
            }
            vips_error_clear();
            g_object_unref(image);
            continue; // Try next quality
        }
        
        g_object_unref(image);
        image = processed_image;
        processed_image = NULL;
        
        // Compress with current quality
        vips_error_clear();
        int save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
            "Q", quality,
            "optimize_coding", TRUE,
            //  "strip", TRUE,
            NULL);
        
        if (save_result == 0 && buffer && buffer_size > 0) {
            // Calculate size in KB
            int size_kb = (int)(buffer_size / 1024);
            printf("[image_compressor] Quality %d: %d KB\n", quality, size_kb);
            
            // Check if size is within target range
            if (size_kb <= up_size_buffer_kb && size_kb >= down_size_buffer_kb) {
                // Success! Found optimal quality
                result.data = (uint8_t*)buffer;
                result.length = buffer_size;
                result.success = 1;
                
                printf("[image_compressor] ✅ Smart compression success!\n");
                printf("[image_compressor] Filename: %s\n", strrchr(input_path, '/') ? strrchr(input_path, '/') + 1 : input_path);
                printf("[image_compressor] Final Quality: %d, Size: %d KB\n", quality, size_kb);
                
                g_object_unref(image);
                pthread_mutex_unlock(&vips_mutex);
                return result;
            } else {
                // Size not in range, free buffer and try next quality
                printf("[image_compressor] Size %d KB not in range %d-%d KB, trying next quality\n", 
                       size_kb, down_size_buffer_kb, up_size_buffer_kb);
                g_free(buffer);
                buffer = NULL;
                buffer_size = 0;
            }
        } else {
            printf("[image_compressor] Error: Failed to compress with quality %d\n", quality);
            const char* error = vips_error_buffer();
            if (error && strlen(error) > 0) {
                printf("[image_compressor] VIPS error: %s\n", error);
            }
            vips_error_clear();
        }
        
        // Cleanup for this iteration
        if (buffer) {
            g_free(buffer);
        }
        g_object_unref(image);
    }
    
    // If we get here, no quality setting achieved the target size
    printf("[image_compressor] ❌ Smart compression failed: Could not achieve target size\n");
    printf("[image_compressor] Tried quality range: %d to %d\n", start_quality, end_quality);
    
    pthread_mutex_unlock(&vips_mutex);
    return result;
}

// Format-aware version of compress_large_image
CompressedImageResult compress_large_image_with_format(const char* input_path, int quality, ImageFormat format) {
    CompressedImageResult result = {NULL, 0, -1};
    
    // Auto-detect format if requested
    if (format == FORMAT_AUTO) {
        format = detect_format_from_path(input_path);
        printf("[image_compressor] Auto-detected format: %d\n", format);
    }
    
    printf("[image_compressor] Handling large image with format %d: %s\n", format, input_path);
    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        return result;
    }
    
    pthread_mutex_lock(&vips_mutex);
    
    VipsImage* image = NULL;
    VipsImage* small_image = NULL;
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    // Load image
    vips_error_clear();
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load large image\n");
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get dimensions
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    
    printf("[image_compressor] Large image: %dx%d\n", width, height);
    
    // Create a smaller version for compression (max 6000px)
    const int max_dimension = 6000;
    double scale = 1.0;
    if (width > height) {
        scale = (double)max_dimension / width;
    } else {
        scale = (double)max_dimension / height;
    }
    
    printf("[image_compressor] Creating smaller version with scale: %f\n", scale);
    
    if (vips_resize(image, &small_image, scale, NULL)) {
        printf("[image_compressor] Error: Failed to create smaller version\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    g_object_unref(image);
    image = small_image;
    
    // Convert to sRGB (except for GIF)
    if (format != FORMAT_GIF) {
        VipsImage* srgb_image = NULL;
        if (vips_copy(image, &srgb_image, 
                "interpretation", VIPS_INTERPRETATION_sRGB,
                NULL)) {
            printf("[image_compressor] Error: Failed to convert to sRGB\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        
        g_object_unref(image);
        image = srgb_image;
    }
    
    // Compression with format-specific settings
    printf("[image_compressor] Starting compression with format %d...\n", format);
    vips_error_clear();
    
    int save_result = -1;
    
    switch (format) {
        case FORMAT_JPEG:
            save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "optimize_coding", TRUE,
                "interlace", FALSE,
                "no_subsample", FALSE,
                NULL);
            break;
            
        case FORMAT_PNG:
            // PNG quality is 0-9, convert from 1-100
            int png_quality = (quality * 9) / 100;
            if (png_quality < 0) png_quality = 0;
            if (png_quality > 9) png_quality = 9;
            
            save_result = vips_pngsave_buffer(image, &buffer, &buffer_size,
                "compression", png_quality,
                "interlace", FALSE,
                NULL);
            break;
            
        case FORMAT_WEBP:
            save_result = vips_webpsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                "near_lossless", FALSE,
                "smart_subsample", TRUE,
                NULL);
            break;
            
        case FORMAT_TIFF:
            save_result = vips_tiffsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "compression", VIPS_FOREIGN_TIFF_COMPRESSION_JPEG,
                "predictor", VIPS_FOREIGN_TIFF_PREDICTOR_HORIZONTAL,
                NULL);
            break;
            
        case FORMAT_HEIF:
            save_result = vips_heifsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                NULL);
            break;
            
        case FORMAT_JP2K:
            save_result = vips_jp2ksave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                NULL);
            break;
            
        case FORMAT_JXL:
            save_result = vips_jxlsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                NULL);
            break;
            
        case FORMAT_GIF:
            // GIF doesn't support quality, use default settings
            save_result = vips_gifsave_buffer(image, &buffer, &buffer_size,
                NULL);
            break;
            
        default:
            printf("[image_compressor] Error: Unsupported format %d\n", format);
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
    }
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Large image compression successful: %zu bytes (format: %d)\n", buffer_size, format);
    } else {
        printf("[image_compressor] Error: Large image compression failed for format %d\n", format);
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        
        if (buffer) {
            g_free(buffer);
        }
    }
    
    g_object_unref(image);
    pthread_mutex_unlock(&vips_mutex);
    return result;
}

// Format-aware version of compress_large_dslr_image
CompressedImageResult compress_large_dslr_image_with_format(const char* input_path, int quality, ImageFormat format) {
    CompressedImageResult result = {NULL, 0, -1};
    
    // Auto-detect format if requested
    if (format == FORMAT_AUTO) {
        format = detect_format_from_path(input_path);
        printf("[image_compressor] Auto-detected format: %d\n", format);
    }
    
    printf("[image_compressor] Handling very large DSLR image with format %d: %s\n", format, input_path);
    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        return result;
    }
    
    pthread_mutex_lock(&vips_mutex);
    
    VipsImage* image = NULL;
    VipsImage* small_image = NULL;
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    // Load image
    vips_error_clear();
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load large DSLR image\n");
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get dimensions
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    
    printf("[image_compressor] Large DSLR image: %dx%d\n", width, height);
    
    // Create a smaller version for compression (max 6000px)
    const int max_dimension = 6000;
    double scale = 1.0;
    if (width > height) {
        scale = (double)max_dimension / width;
    } else {
        scale = (double)max_dimension / height;
    }
    
    printf("[image_compressor] Creating smaller version with scale: %f\n", scale);
    
    if (vips_resize(image, &small_image, scale, NULL)) {
        printf("[image_compressor] Error: Failed to create smaller version\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    g_object_unref(image);
    image = small_image;
    
    // Convert to sRGB (except for GIF)
    if (format != FORMAT_GIF) {
        VipsImage* srgb_image = NULL;
        if (vips_copy(image, &srgb_image, 
                "interpretation", VIPS_INTERPRETATION_sRGB,
                NULL)) {
            printf("[image_compressor] Error: Failed to convert to sRGB\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        
        g_object_unref(image);
        image = srgb_image;
    }
    
    // Compression with format-specific settings
    printf("[image_compressor] Starting DSLR compression with format %d...\n", format);
    vips_error_clear();
    
    int save_result = -1;
    
    switch (format) {
        case FORMAT_JPEG:
            save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "optimize_coding", TRUE,
                "interlace", FALSE,
                "no_subsample", FALSE,
                NULL);
            break;
            
        case FORMAT_PNG:
            // PNG quality is 0-9, convert from 1-100
            int png_quality = (quality * 9) / 100;
            if (png_quality < 0) png_quality = 0;
            if (png_quality > 9) png_quality = 9;
            
            save_result = vips_pngsave_buffer(image, &buffer, &buffer_size,
                "compression", png_quality,
                "interlace", FALSE,
                NULL);
            break;
            
        case FORMAT_WEBP:
            save_result = vips_webpsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                "near_lossless", FALSE,
                "smart_subsample", TRUE,
                NULL);
            break;
            
        case FORMAT_TIFF:
            save_result = vips_tiffsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "compression", VIPS_FOREIGN_TIFF_COMPRESSION_JPEG,
                "predictor", VIPS_FOREIGN_TIFF_PREDICTOR_HORIZONTAL,
                NULL);
            break;
            
        case FORMAT_HEIF:
            save_result = vips_heifsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                NULL);
            break;
            
        case FORMAT_JP2K:
            save_result = vips_jp2ksave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                NULL);
            break;
            
        case FORMAT_JXL:
            save_result = vips_jxlsave_buffer(image, &buffer, &buffer_size,
                "Q", quality,
                "lossless", FALSE,
                NULL);
            break;
            
        case FORMAT_GIF:
            // GIF doesn't support quality, use default settings
            save_result = vips_gifsave_buffer(image, &buffer, &buffer_size,
                NULL);
            break;
            
        default:
            printf("[image_compressor] Error: Unsupported format %d\n", format);
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
    }
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Large DSLR image compression successful: %zu bytes (format: %d)\n", buffer_size, format);
    } else {
        printf("[image_compressor] Error: Large DSLR image compression failed for format %d\n", format);
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        
        if (buffer) {
            g_free(buffer);
        }
    }
    
    g_object_unref(image);
    pthread_mutex_unlock(&vips_mutex);
    return result;
}

// Format-aware version of smart_compress_image
CompressedImageResult smart_compress_image_with_format(const char* input_path, int target_kb, int type, ImageFormat format) {
    CompressedImageResult result = {NULL, 0, -1};
    
    // Auto-detect format if requested
    if (format == FORMAT_AUTO) {
        format = detect_format_from_path(input_path);
        printf("[image_compressor] Auto-detected format: %d\n", format);
    }
    
    printf("[image_compressor] Smart compression with format %d: %s (target: %d KB, type: %d)\n", 
           format, input_path, target_kb, type);
    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        return result;
    }
    
    pthread_mutex_lock(&vips_mutex);
    
    VipsImage* image = NULL;
    VipsImage* processed_image = NULL;
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    // Load image
    vips_error_clear();
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load image for smart compression\n");
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get dimensions
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    
    printf("[image_compressor] Image: %dx%d\n", width, height);
    
    // Smart compression logic based on type
    int target_quality = 85; // Default quality
    int needs_resize = 0;
    double scale = 1.0;
    
    switch (type) {
        case 0: // Standard compression
            target_quality = 85;
            break;
        case 1: // High quality
            target_quality = 95;
            break;
        case 2: // Low quality
            target_quality = 60;
            break;
        case 3: // Very low quality
            target_quality = 30;
            break;
        default:
            target_quality = 85;
            break;
    }
    
    // Resize if needed for target size
    if (target_kb > 0) {
        // Estimate if resize is needed based on target size
        long estimated_size = (long)width * height * 3; // Rough estimate
        if (estimated_size > target_kb * 1024) {
            needs_resize = 1;
            scale = sqrt((double)(target_kb * 1024) / estimated_size);
            scale = fmax(0.1, fmin(1.0, scale)); // Clamp between 0.1 and 1.0
        }
    }
    
    if (needs_resize) {
        printf("[image_compressor] Resizing with scale: %f\n", scale);
        if (vips_resize(image, &processed_image, scale, 
                "kernel", VIPS_KERNEL_LANCZOS3,
                NULL)) {
            printf("[image_compressor] Error: Failed to resize image\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        g_object_unref(image);
        image = processed_image;
        processed_image = NULL;
    }
    
    // Convert to sRGB (except for GIF)
    if (format != FORMAT_GIF) {
        VipsImage* srgb_image = NULL;
        if (vips_copy(image, &srgb_image, 
                "interpretation", VIPS_INTERPRETATION_sRGB,
                NULL)) {
            printf("[image_compressor] Error: Failed to convert to sRGB\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        
        g_object_unref(image);
        image = srgb_image;
    }
    
    // Compression with format-specific settings
    printf("[image_compressor] Starting smart compression with format %d, quality %d...\n", format, target_quality);
    vips_error_clear();
    
    int save_result = -1;
    
    switch (format) {
        case FORMAT_JPEG:
            save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
                "Q", target_quality,
                "optimize_coding", TRUE,
                "interlace", FALSE,
                "no_subsample", FALSE,
                NULL);
            break;
            
        case FORMAT_PNG:
            // PNG quality is 0-9, convert from 1-100
            int png_quality = (target_quality * 9) / 100;
            if (png_quality < 0) png_quality = 0;
            if (png_quality > 9) png_quality = 9;
            
            save_result = vips_pngsave_buffer(image, &buffer, &buffer_size,
                "compression", png_quality,
                "interlace", FALSE,
                NULL);
            break;
            
        case FORMAT_WEBP:
            save_result = vips_webpsave_buffer(image, &buffer, &buffer_size,
                "Q", target_quality,
                "lossless", FALSE,
                "near_lossless", FALSE,
                "smart_subsample", TRUE,
                NULL);
            break;
            
        case FORMAT_TIFF:
            save_result = vips_tiffsave_buffer(image, &buffer, &buffer_size,
                "Q", target_quality,
                "compression", VIPS_FOREIGN_TIFF_COMPRESSION_JPEG,
                "predictor", VIPS_FOREIGN_TIFF_PREDICTOR_HORIZONTAL,
                NULL);
            break;
            
        case FORMAT_HEIF:
            save_result = vips_heifsave_buffer(image, &buffer, &buffer_size,
                "Q", target_quality,
                "lossless", FALSE,
                NULL);
            break;
            
        case FORMAT_JP2K:
            save_result = vips_jp2ksave_buffer(image, &buffer, &buffer_size,
                "Q", target_quality,
                "lossless", FALSE,
                NULL);
            break;
            
        case FORMAT_JXL:
            save_result = vips_jxlsave_buffer(image, &buffer, &buffer_size,
                "Q", target_quality,
                "lossless", FALSE,
                NULL);
            break;
            
        case FORMAT_GIF:
            // GIF doesn't support quality, use default settings
            save_result = vips_gifsave_buffer(image, &buffer, &buffer_size,
                NULL);
            break;
            
        default:
            printf("[image_compressor] Error: Unsupported format %d\n", format);
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
    }
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Smart compression successful: %zu bytes (format: %d, quality: %d)\n", 
               buffer_size, format, target_quality);
    } else {
        printf("[image_compressor] Error: Smart compression failed for format %d\n", format);
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        
        if (buffer) {
            g_free(buffer);
        }
    }
    
    g_object_unref(image);
    pthread_mutex_unlock(&vips_mutex);
    return result;
}

// Auto-compress function that tries multiple formats to find the smallest file
CompressedImageResult auto_compress_image(const char* input_path, int quality) {
    CompressedImageResult result = {NULL, 0, -1};
    
    // Input validation
    if (!input_path || strlen(input_path) == 0) {
        printf("[image_compressor] Error: Invalid input path\n");
        return result;
    }
    
    if (quality < 1 || quality > 100) {
        printf("[image_compressor] Error: Quality must be between 1 and 100\n");
        return result;
    }
    
    printf("[image_compressor] Auto-compressing image: %s (quality: %d)\n", input_path, quality);
    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        printf("[image_compressor] Error: VIPS initialization failed\n");
        return result;
    }
    
    // Lock VIPS operations for this thread
    pthread_mutex_lock(&vips_mutex);
    
    // Clear any previous errors
    vips_error_clear();
    
    VipsImage* image = NULL;
    VipsImage* processed_image = NULL;
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    // Load image with error handling
    printf("[image_compressor] Loading image...\n");
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load image\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        vips_error_clear();
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Validate image object
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image object\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get image info
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    int bands = vips_image_get_bands(image);
    
    printf("[image_compressor] Image loaded: %dx%d, %d bands\n", width, height, bands);
    
    // Validate image dimensions
    if (width <= 0 || height <= 0 || bands <= 0) {
        printf("[image_compressor] Error: Invalid image dimensions\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Compression strategy based on dimensions only
    int new_width = width;
    int new_height = height;
    int needs_resize = 0;
    
    // Resize if largest dimension > 6000px
    const int max_dimension = 6000;
    if (width > max_dimension || height > max_dimension) {
        needs_resize = 1;
        if (width > height) {
            new_width = max_dimension;
            new_height = (int)((double)height * max_dimension / width);
        } else {
            new_height = max_dimension;
            new_width = (int)((double)width * max_dimension / height);
        }
        printf("[image_compressor] Resizing from %dx%d to %dx%d\n", width, height, new_width, new_height);
    }
    
    // Process image (resize if needed and convert to sRGB)
    vips_error_clear();
    
    if (needs_resize) {
        printf("[image_compressor] Resizing image with high quality...\n");
        double scale = 1.0;
        if (width > height) {
            scale = (double)max_dimension / width;
        } else {
            scale = (double)max_dimension / height;
        }
        
        printf("[image_compressor] Scale factor: %f\n", scale);
        
        if (vips_resize(image, &processed_image, scale, 
                "kernel", VIPS_KERNEL_LANCZOS3,  // High-quality kernel
                NULL)) {
            printf("[image_compressor] Error: Failed to resize image\n");
            const char* error = vips_error_buffer();
            if (error && strlen(error) > 0) {
                printf("[image_compressor] VIPS error: %s\n", error);
            }
            vips_error_clear();
            
            // Try to compress original image without resizing
            printf("[image_compressor] Trying to compress original image without resizing...\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        g_object_unref(image);
        image = processed_image;
        processed_image = NULL;
        
        // Get new dimensions
        width = vips_image_get_width(image);
        height = vips_image_get_height(image);
        printf("[image_compressor] Image resized to: %dx%d\n", width, height);
        
        // Validate resized image
        if (width <= 0 || height <= 0) {
            printf("[image_compressor] Error: Invalid dimensions after resize\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
    }
    
    // Convert to sRGB for consistent color space
    printf("[image_compressor] Converting image to sRGB...\n");
    vips_error_clear();
    if (vips_copy(image, &processed_image, 
            "interpretation", VIPS_INTERPRETATION_sRGB,
            NULL)) {
        printf("[image_compressor] Error: Failed to convert image to sRGB\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        vips_error_clear();
        
        // Try to compress without sRGB conversion
        printf("[image_compressor] Trying to compress image without sRGB conversion...\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    g_object_unref(image);
    image = processed_image;
    processed_image = NULL;
    
    // Validate final image before compression
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image after processing\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    int final_width = vips_image_get_width(image);
    int final_height = vips_image_get_height(image);
    int final_bands = vips_image_get_bands(image);
    printf("[image_compressor] Final image: %dx%d, %d bands\n", final_width, final_height, final_bands);
    
    // Try different formats to find the smallest file
    CompressedImageResult best_result = {NULL, 0, -1};
    size_t best_size = SIZE_MAX;
    ImageFormat best_format = FORMAT_JPEG;
    
    // Define formats to try in order of preference for size
    ImageFormat formats_to_try[] = {
        FORMAT_WEBP,    // Usually smallest for photos
        FORMAT_JPEG,    // Good for photos
        FORMAT_JXL,     // Excellent compression
        FORMAT_HEIF,    // Good compression
        FORMAT_JP2K,    // Good compression
        FORMAT_TIFF,    // Good for some images
        FORMAT_PNG,     // Lossless, usually larger
        FORMAT_GIF      // Only for animated images
    };
    
    int num_formats = sizeof(formats_to_try) / sizeof(formats_to_try[0]);
    
    for (int i = 0; i < num_formats; i++) {
        ImageFormat current_format = formats_to_try[i];
        printf("[image_compressor] Trying format %d...\n", current_format);
        
        // Clear any existing buffer
        if (buffer) {
            g_free(buffer);
            buffer = NULL;
            buffer_size = 0;
        }
        
        vips_error_clear();
        int save_result = -1;
        
        switch (current_format) {
            case FORMAT_JPEG:
                save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
                    "Q", quality,
                    "optimize_coding", TRUE,
                    "interlace", FALSE,
                    "no_subsample", FALSE,
                    //"strip", FALSE,  // Keep orientation data
                    NULL);
                break;
                
            case FORMAT_PNG:
                // PNG quality is 0-9, convert from 1-100
                int png_quality = 9 - ((quality * 9) / 100);
                if (png_quality < 0) png_quality = 0;
                if (png_quality > 9) png_quality = 9;
                
                save_result = vips_pngsave_buffer(image, &buffer, &buffer_size,
                    "compression", png_quality,
                    "interlace", FALSE,
                    //"strip", FALSE,  // Keep orientation data
                    NULL);
                break;
                
            case FORMAT_WEBP:
                save_result = vips_webpsave_buffer(image, &buffer, &buffer_size,
                    "Q", quality,
                    "lossless", FALSE,
                    "near_lossless", FALSE,
                    "smart_subsample", FALSE,  // Disable for faster compression
                    "strip", FALSE,  // Keep orientation data
                    "effort", 2,     // Lower effort for faster compression (0-6, default is 4)
                    NULL);
                break;
                
            case FORMAT_TIFF:
                save_result = vips_tiffsave_buffer(image, &buffer, &buffer_size,
                    "Q", quality,
                    "compression", VIPS_FOREIGN_TIFF_COMPRESSION_JPEG,
                    "predictor", VIPS_FOREIGN_TIFF_PREDICTOR_HORIZONTAL,
                    //"strip", FALSE,  // Keep orientation data
                    NULL);
                break;
                
            case FORMAT_HEIF:
                save_result = vips_heifsave_buffer(image, &buffer, &buffer_size,
                    "Q", quality,
                    "lossless", FALSE,
                    //"strip", FALSE,  // Keep orientation data
                    NULL);
                break;
                
            case FORMAT_JP2K:
                save_result = vips_jp2ksave_buffer(image, &buffer, &buffer_size,
                    "Q", quality,
                    "lossless", FALSE,
                    //"strip", FALSE,  // Keep orientation data
                    NULL);
                break;
                
            case FORMAT_JXL:
                save_result = vips_jxlsave_buffer(image, &buffer, &buffer_size,
                    "Q", quality,
                    "lossless", FALSE,
                    //"strip", FALSE,  // Keep orientation data
                    NULL);
                break;
                
            case FORMAT_GIF:
                // Only try GIF if image has multiple bands (might be animated)
                if (final_bands >= 3) {
                    save_result = vips_gifsave_buffer(image, &buffer, &buffer_size,
                        //"strip", FALSE,  // Keep orientation data
                        NULL);
                } else {
                    save_result = -1; // Skip GIF for non-animated images
                }
                break;
                
            default:
                save_result = -1;
                break;
        }
        
        if (save_result == 0 && buffer && buffer_size > 0) {
            printf("[image_compressor] Format %d successful: %zu bytes\n", current_format, buffer_size);
            
            // Check if this is the smallest so far
            if (buffer_size < best_size) {
                // Free previous best result
                if (best_result.data) {
                    g_free(best_result.data);
                }
                
                // Update best result
                best_result.data = (uint8_t*)buffer;
                best_result.length = buffer_size;
                best_result.success = 1;
                best_size = buffer_size;
                best_format = current_format;
                
                // Don't free buffer here, it's now owned by best_result
                buffer = NULL;
                buffer_size = 0;
                
                printf("[image_compressor] New best format: %d with %zu bytes\n", best_format, best_size);
            } else {
                // Free this buffer since it's not the best
                g_free(buffer);
                buffer = NULL;
                buffer_size = 0;
            }
        } else {
            printf("[image_compressor] Format %d failed\n", current_format);
            const char* error = vips_error_buffer();
            if (error && strlen(error) > 0) {
                printf("[image_compressor] VIPS error: %s\n", error);
            }
            vips_error_clear();
        }
    }
    
    // Cleanup
    if (buffer) {
        g_free(buffer);
    }
    g_object_unref(image);
    
    if (best_result.success == 1) {
        printf("[image_compressor] Auto-compression successful: %zu bytes (best format: %d)\n", 
               best_size, best_format);
        result = best_result;
    } else {
        printf("[image_compressor] Error: All formats failed\n");
    }
    
    pthread_mutex_unlock(&vips_mutex);
    return result;
}

// Fast WebP compression for speed-critical applications
CompressedImageResult fast_webp_compress(const char* input_path, int quality) {
    CompressedImageResult result = {NULL, 0, -1};
    
    // Input validation
    if (!input_path || strlen(input_path) == 0) {
        printf("[image_compressor] Error: Invalid input path\n");
        return result;
    }
    
    if (quality < 1 || quality > 100) {
        printf("[image_compressor] Error: Quality must be between 1 and 100\n");
        return result;
    }
    
    printf("[image_compressor] Fast WebP compression: %s (quality: %d)\n", input_path, quality);
    
    // Initialize VIPS (thread-safe)
    if (!ensure_vips_initialized()) {
        printf("[image_compressor] Error: VIPS initialization failed\n");
        return result;
    }
    
    // Lock VIPS operations for this thread
    pthread_mutex_lock(&vips_mutex);
    
    // Clear any previous errors
    vips_error_clear();
    
    VipsImage* image = NULL;
    VipsImage* processed_image = NULL;
    void* buffer = NULL;
    size_t buffer_size = 0;
    
    // Load image with error handling
    printf("[image_compressor] Loading image...\n");
    image = vips_image_new_from_file(input_path, 
        "fail_on", VIPS_FAIL_ON_NONE,
        "access", VIPS_ACCESS_SEQUENTIAL,
        NULL);
    
    if (!image) {
        printf("[image_compressor] Error: Failed to load image\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
        vips_error_clear();
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Validate image object
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image object\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get image info
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    int bands = vips_image_get_bands(image);
    
    printf("[image_compressor] Image loaded: %dx%d, %d bands\n", width, height, bands);
    
    // Validate image dimensions
    if (width <= 0 || height <= 0 || bands <= 0) {
        printf("[image_compressor] Error: Invalid image dimensions\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Fast compression strategy - minimal processing
    int needs_resize = 0;
    
    // Only resize if absolutely necessary (very large images)
    const int max_dimension = 8000; // Higher threshold for fast compression
    if (width > max_dimension || height > max_dimension) {
        needs_resize = 1;
        double scale = 1.0;
        if (width > height) {
            scale = (double)max_dimension / width;
        } else {
            scale = (double)max_dimension / height;
        }
        
        printf("[image_compressor] Fast resize with scale: %f\n", scale);
        
        if (vips_resize(image, &processed_image, scale, 
                "kernel", VIPS_KERNEL_LINEAR,  // Use faster kernel
                NULL)) {
            printf("[image_compressor] Error: Failed to resize image\n");
            g_object_unref(image);
            pthread_mutex_unlock(&vips_mutex);
            return result;
        }
        g_object_unref(image);
        image = processed_image;
        processed_image = NULL;
    }
    
    // Convert to sRGB for consistent color space
    printf("[image_compressor] Converting image to sRGB...\n");
    vips_error_clear();
    if (vips_copy(image, &processed_image, 
            "interpretation", VIPS_INTERPRETATION_sRGB,
            NULL)) {
        printf("[image_compressor] Error: Failed to convert image to sRGB\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    g_object_unref(image);
    image = processed_image;
    processed_image = NULL;
    
    // Validate final image before compression
    if (!VIPS_IS_IMAGE(image)) {
        printf("[image_compressor] Error: Invalid image after processing\n");
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Fast WebP compression with optimized settings
    printf("[image_compressor] Starting fast WebP compression...\n");
    vips_error_clear();
    
    int save_result = vips_webpsave_buffer(image, &buffer, &buffer_size,
        "Q", quality,
        "lossless", FALSE,
        "near_lossless", FALSE,  // Disable for speed
        "smart_subsample", FALSE,  // Disable for speed
        "strip", FALSE,  // Keep orientation data
        "effort", 1,     // Minimum effort for maximum speed (0-6)
        "method", 0,     // Fastest method (0-6)
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        // Compression successful
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 1;
        printf("[image_compressor] Fast WebP compression successful: %zu bytes (quality: %d)\n", 
               buffer_size, quality);
        g_object_unref(image);
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Compression failed
    printf("[image_compressor] Error: Fast WebP compression failed\n");
    const char* error = vips_error_buffer();
    if (error && strlen(error) > 0) {
        printf("[image_compressor] VIPS error: %s\n", error);
    }
    
    // Cleanup
    if (buffer) {
        g_free(buffer);
    }
    g_object_unref(image);
    vips_error_clear();
    
    pthread_mutex_unlock(&vips_mutex);
    return result;
}