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

// Global flag to track VIPS initialization with mutex protection
static int vips_initialized = 0;
static pthread_mutex_t vips_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
CompressedImageResult compress_large_dslr_image(const char* input_path, int quality);
CompressedImageResult compress_large_image(const char* input_path, int quality);
CompressedImageResult compress_extremely_large_image(const char* input_path, int quality);

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
    
    // For extremely large images, use special handler
    if (file_size > 20000000) { // >20MB
        printf("[image_compressor] Extremely large image detected, using special handler\n");
        return compress_extremely_large_image(input_path, quality);
    }
    
    // For very large images, use large image handler
    if (file_size > 8000000) { // >8MB
        printf("[image_compressor] Very large image detected, using large image handler\n");
        return compress_large_image(input_path, quality);
    }
    
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
    
    // Enhanced compression strategy based on file size and dimensions
    int new_width = width;
    int new_height = height;
    int needs_resize = 0;
    int is_very_large = 0;
    int is_extremely_large = 0;
    
    // Determine compression strategy based on file size and dimensions
    if (file_size > 15000000) { // >15MB
        is_extremely_large = 1;
        printf("[image_compressor] Extremely large image detected: %ld bytes\n", file_size);
        printf("[image_compressor] Using aggressive compression strategy\n");
    } else if (file_size > 8000000) { // >8MB
        is_very_large = 1;
        printf("[image_compressor] Very large image detected: %ld bytes\n", file_size);
        printf("[image_compressor] Using enhanced compression strategy\n");
    }
    
    // Resizing strategy based on file size and dimensions
    int max_dimension = 6000; // Default max dimension
    
    if (is_extremely_large) {
        // For extremely large images (>15MB), be very aggressive
        max_dimension = 3000;
        printf("[image_compressor] Using aggressive resizing: max %dpx\n", max_dimension);
    } else if (is_very_large) {
        // For very large images (>8MB), be moderately aggressive
        max_dimension = 4000;
        printf("[image_compressor] Using enhanced resizing: max %dpx\n", max_dimension);
    } else if (width > 6000 || height > 6000) {
        // Standard large image handling
        max_dimension = 6000;
        needs_resize = 1;
    }
    
    // Apply resizing if needed
    if (width > max_dimension || height > max_dimension || is_very_large || is_extremely_large) {
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
    
    // Enhanced compression with quality control based on file size
    printf("[image_compressor] Starting enhanced compression...\n");
    vips_error_clear();
    
    // Adjust quality based on original file size and user quality
    int final_quality = quality;
    
    if (is_extremely_large) {
        // For extremely large images, be very aggressive with quality
        final_quality = quality > 60 ? 60 : quality;
        printf("[image_compressor] Extremely large image, limiting quality to %d\n", final_quality);
    } else if (is_very_large) {
        // For very large images, be moderately aggressive
        final_quality = quality > 75 ? 75 : quality;
        printf("[image_compressor] Very large image, limiting quality to %d\n", final_quality);
    } else if (final_width * final_height > 16000000) { // >16MP
        final_quality = quality > 85 ? 85 : quality;
        printf("[image_compressor] High-res image, optimizing quality to %d\n", final_quality);
    } else if (final_width * final_height > 8000000) { // >8MP
        final_quality = quality > 90 ? 90 : quality;
        printf("[image_compressor] Medium-res image, using quality %d\n", final_quality);
    }
    
    // Enhanced JPEG save options for better compression
    int save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
        "Q", final_quality,
        "strip", TRUE,
        "optimize_coding", TRUE,
        "interlace", FALSE,  // Better for most images
        "no_subsample", FALSE, // Allow subsampling for better compression
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        // Compression successful
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 0;
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
        "strip", TRUE,
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 0;
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
    
    // Get file size to determine strategy
    FILE* file = fopen(input_path, "rb");
    if (!file) {
        printf("[image_compressor] Error: Cannot open file\n");
        return result;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);
    
    printf("[image_compressor] Large image file size: %ld bytes\n", file_size);
    
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
    
    // Determine max dimension based on file size
    int max_dimension = 6000; // Default
    if (file_size > 15000000) { // >15MB
        max_dimension = 2500; // Very aggressive
        printf("[image_compressor] Extremely large file, using max %dpx\n", max_dimension);
    } else if (file_size > 8000000) { // >8MB
        max_dimension = 3500; // Moderately aggressive
        printf("[image_compressor] Very large file, using max %dpx\n", max_dimension);
    }
    
    // Create a smaller version for compression
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
    
    // Adjust quality based on file size
    int final_quality = quality;
    if (file_size > 15000000) {
        final_quality = quality > 50 ? 50 : quality; // Very aggressive
        printf("[image_compressor] Extremely large file, limiting quality to %d\n", final_quality);
    } else if (file_size > 8000000) {
        final_quality = quality > 65 ? 65 : quality; // Moderately aggressive
        printf("[image_compressor] Very large file, limiting quality to %d\n", final_quality);
    }
    
    printf("[image_compressor] Compressing with quality: %d\n", final_quality);
    
    vips_error_clear();
    int save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
        "Q", final_quality,
        "strip", TRUE,
        "optimize_coding", TRUE,
        "interlace", FALSE,
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 0;
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
    
    // Get file size to determine strategy
    FILE* file = fopen(input_path, "rb");
    if (!file) {
        printf("[image_compressor] Error: Cannot open file\n");
        return result;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);
    
    printf("[image_compressor] Large DSLR image file size: %ld bytes\n", file_size);
    
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
    
    // Determine max dimension based on file size for DSLR
    int max_dimension = 6000; // Default
    if (file_size > 15000000) { // >15MB
        max_dimension = 2500; // Very aggressive for DSLR
        printf("[image_compressor] Extremely large DSLR file, using max %dpx\n", max_dimension);
    } else if (file_size > 8000000) { // >8MB
        max_dimension = 3500; // Moderately aggressive for DSLR
        printf("[image_compressor] Very large DSLR file, using max %dpx\n", max_dimension);
    }
    
    // Create a smaller version for compression
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
    
    // Adjust quality based on file size for DSLR
    int final_quality = quality;
    if (file_size > 15000000) {
        final_quality = quality > 55 ? 55 : quality; // Very aggressive for DSLR
        printf("[image_compressor] Extremely large DSLR file, limiting quality to %d\n", final_quality);
    } else if (file_size > 8000000) {
        final_quality = quality > 70 ? 70 : quality; // Moderately aggressive for DSLR
        printf("[image_compressor] Very large DSLR file, limiting quality to %d\n", final_quality);
    }
    
    printf("[image_compressor] Compressing DSLR with quality: %d\n", final_quality);
    
    vips_error_clear();
    int save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
        "Q", final_quality,
        "strip", TRUE,
        "optimize_coding", TRUE,
        "interlace", FALSE,  // Better for DSLR images
        "no_subsample", FALSE, // Allow subsampling for better compression
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 0;
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

// Function to handle extremely large images with very aggressive compression
CompressedImageResult compress_extremely_large_image(const char* input_path, int quality) {
    CompressedImageResult result = {NULL, 0, -1};
    
    printf("[image_compressor] Handling extremely large image: %s\n", input_path);
    
    // Get file size
    FILE* file = fopen(input_path, "rb");
    if (!file) {
        printf("[image_compressor] Error: Cannot open file\n");
        return result;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);
    
    printf("[image_compressor] Extremely large image file size: %ld bytes\n", file_size);
    
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
        printf("[image_compressor] Error: Failed to load extremely large image\n");
        pthread_mutex_unlock(&vips_mutex);
        return result;
    }
    
    // Get dimensions
    int width = vips_image_get_width(image);
    int height = vips_image_get_height(image);
    
    printf("[image_compressor] Extremely large image: %dx%d\n", width, height);
    
    // Very aggressive resizing for extremely large images
    int max_dimension = 2000; // Very small for extremely large images
    if (file_size > 25000000) { // >25MB
        max_dimension = 1500; // Extremely aggressive
        printf("[image_compressor] Massive file, using max %dpx\n", max_dimension);
    }
    
    // Create a much smaller version for compression
    double scale = 1.0;
    if (width > height) {
        scale = (double)max_dimension / width;
    } else {
        scale = (double)max_dimension / height;
    }
    
    printf("[image_compressor] Creating much smaller version with scale: %f\n", scale);
    
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
    
    // Very aggressive quality for extremely large images
    int final_quality = quality > 40 ? 40 : quality; // Very low quality
    printf("[image_compressor] Extremely large image, using very low quality: %d\n", final_quality);
    
    vips_error_clear();
    int save_result = vips_jpegsave_buffer(image, &buffer, &buffer_size,
        "Q", final_quality,
        "strip", TRUE,
        "optimize_coding", TRUE,
        "interlace", FALSE,
        NULL);
    
    if (save_result == 0 && buffer && buffer_size > 0) {
        result.data = (uint8_t*)buffer;
        result.length = buffer_size;
        result.success = 0;
        printf("[image_compressor] Extremely large image compression successful: %zu bytes\n", buffer_size);
    } else {
        printf("[image_compressor] Error: Failed to compress extremely large image\n");
        const char* error = vips_error_buffer();
        if (error && strlen(error) > 0) {
            printf("[image_compressor] VIPS error: %s\n", error);
        }
    }
    
    g_object_unref(image);
    pthread_mutex_unlock(&vips_mutex);
    return result;
} 