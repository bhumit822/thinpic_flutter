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
CompressedImageResult smart_compress_image(const char* input_path, int target_kb, int type);

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