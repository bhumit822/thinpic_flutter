# thinpic_flutter

A high-performance Flutter FFI plugin for image compression using the VIPS image processing library. This plugin provides efficient image compression with various optimization strategies for different use cases.

## Features

- **Multiple Compression Strategies**: Basic, size-constrained, large image, DSLR, and smart compression
- **Cross-Platform**: Works on Android, iOS, macOS, Linux, and Windows
- **High Performance**: Native C implementation using VIPS library
- **Memory Efficient**: Automatic buffer management and cleanup
- **Thread-Safe**: Thread-safe operations with proper synchronization
- **Quality Control**: Precise quality settings from 1-100
- **Smart Compression**: Automatically adjusts quality to meet target file sizes (be cautious when using this method).

## Installation

Add this to your `pubspec.yaml`:

```yaml
dependencies:
  thinpic_flutter: ^0.0.1
```

## API Reference

### Core Compression Functions

#### `compressImage(String inputPath, int quality)`

Basic image compression with quality control.

**Parameters:**
- `inputPath` (String): Path to the input image file
- `quality` (int): Compression quality from 1-100 (1 = lowest, 100 = highest)

**Returns:** `CompressedImageResult`

**Example:**
```dart
import 'package:thinpic_flutter/thinpic_flutter.dart';

final result = compressImage('/path/to/image.jpg', 80);

if (isCompressionSuccessful(result)) {
  final bytes = compressedResultToBytes(result);
  // Use the compressed image bytes
  freeCompressedBuffer(result.data); // Don't forget to free the buffer
}
```

#### `compressImageWithSize(String inputPath, int quality, int targetWidth, int targetHeight)`

Advanced compression with optional size parameters and aspect ratio preservation.

**Parameters:**
- `inputPath` (String): Path to the input image file
- `quality` (int): Compression quality from 1-100
- `targetWidth` (int): Target width in pixels (0 = no constraint)
- `targetHeight` (int): Target height in pixels (0 = no constraint)

**Resize Behavior:**
- If both width and height are provided, the smallest dimension is used to maintain aspect ratio
- If only width is provided, height is calculated to maintain aspect ratio
- If only height is provided, width is calculated to maintain aspect ratio
- If both are 0 or negative, no resizing is applied (same as basic compression)

**Returns:** `CompressedImageResult`

**Example:**
```dart
// Width only - height calculated automatically
final result1 = compressImageWithSize('/path/to/image.jpg', 80, 1920, 0);

// Height only - width calculated automatically
final result2 = compressImageWithSize('/path/to/image.jpg', 80, 0, 1080);

// Both dimensions - smallest used to maintain aspect ratio
final result3 = compressImageWithSize('/path/to/image.jpg', 80, 1920, 1080);

// No resizing (same as basic compression)
final result4 = compressImageWithSize('/path/to/image.jpg', 80, 0, 0);
```

#### `compressLargeImage(String inputPath, int quality)`

Optimized compression for large images with enhanced memory management.

**Parameters:**
- `inputPath` (String): Path to the input image file
- `quality` (int): Compression quality from 1-100

**Returns:** `CompressedImageResult`

**Use Case:** Best for images larger than 5MB or high-resolution photos.

**Example:**
```dart
final result = compressLargeImage('/path/to/large_image.jpg', 85);
```

#### `compressLargeDslrImage(String inputPath, int quality)`

Specialized compression optimized for DSLR camera images with enhanced quality preservation.

**Parameters:**
- `inputPath` (String): Path to the input image file
- `quality` (int): Compression quality from 1-100

**Returns:** `CompressedImageResult`

**Use Case:** Best for professional DSLR photos that need high-quality compression.

**Example:**
```dart
final result = compressLargeDslrImage('/path/to/dslr_photo.jpg', 90);
```

#### `smartCompressImage(String inputPath, int targetKb, int type)`

Smart compression that automatically adjusts quality to meet target file size.

**Parameters:**
- `inputPath` (String): Path to the input image file
- `targetKb` (int): Target file size in kilobytes
- `type` (int): Compression type (1 = high quality, 0 = low quality)

**Quality Strategy:**
- **High Quality (type = 1)**: Starts at 93% quality, applies 1.3x resize, targets 20% tolerance
- **Low Quality (type = 0)**: Starts at 85% quality, no resize, targets 20% tolerance

**Returns:** `CompressedImageResult`

**Example:**
```dart
// High quality compression targeting 500KB
final result1 = smartCompressImage('/path/to/image.jpg', 500, 1);

// Low quality compression targeting 200KB
final result2 = smartCompressImage('/path/to/image.jpg', 200, 0);
```

### Utility Functions

#### `getImageInfo(String inputPath)`

Retrieves detailed information about an image without compression.

**Parameters:**
- `inputPath` (String): Path to the input image file

**Returns:** `ImageInfo`

**ImageInfo Properties:**
- `width` (int): Image width in pixels
- `height` (int): Image height in pixels
- `bands` (int): Number of color bands (3 for RGB, 4 for RGBA, etc.)
- `orientation` (int): Image orientation metadata
- `needs_resize` (int): Whether resizing is recommended
- `new_width` (int): Recommended new width
- `new_height` (int): Recommended new height

**Example:**
```dart
final info = getImageInfo('/path/to/image.jpg');
final (width, height) = getImageDimensions(info);
print('Image dimensions: ${width}x${height}');
```

#### `isCompressionSuccessful(CompressedImageResult result)`

Checks if compression was successful.

**Parameters:**
- `result` (CompressedImageResult): Result from any compression function

**Returns:** `bool`

**Example:**
```dart
final result = compressImage('/path/to/image.jpg', 80);
if (isCompressionSuccessful(result)) {
  // Compression succeeded
} else {
  // Compression failed
}
```

#### `compressedResultToBytes(CompressedImageResult result)`

Converts compression result to Dart bytes.

**Parameters:**
- `result` (CompressedImageResult): Result from any compression function

**Returns:** `Uint8List`

**Example:**
```dart
final result = compressImage('/path/to/image.jpg', 80);
if (isCompressionSuccessful(result)) {
  final bytes = compressedResultToBytes(result);
  // Use bytes for file writing, network upload, etc.
}
```

#### `freeCompressedBuffer(Pointer<Uint8> buffer)`

Frees the native memory buffer to prevent memory leaks.

**Parameters:**
- `buffer` (Pointer<Uint8>): Buffer pointer from compression result

**Example:**
```dart
final result = compressImage('/path/to/image.jpg', 80);
// Use the result...
freeCompressedBuffer(result.data); // Always free the buffer when done
```

#### `getImageDimensions(ImageInfo info)`

Extracts width and height from ImageInfo as a tuple.

**Parameters:**
- `info` (ImageInfo): Image information from getImageInfo

**Returns:** `(int width, int height)`

**Example:**
```dart
final info = getImageInfo('/path/to/image.jpg');
final (width, height) = getImageDimensions(info);
```

#### `shutdownVips()`

Shuts down the VIPS library. Call this when your app is terminating.

**Example:**
```dart
// Call this in your app's dispose or shutdown method
shutdownVips();
```

#### `testVipsBasic()`

Tests basic VIPS functionality. Returns 1 if successful, 0 if failed.

**Returns:** `int`

**Example:**
```dart
final testResult = testVipsBasic();
if (testResult == 1) {
  print('VIPS is working correctly');
} else {
  print('VIPS test failed');
}
```

## Data Structures

### CompressedImageResult

```dart
class CompressedImageResult {
  Pointer<Uint8> data;    // Pointer to compressed image data
  int length;             // Length of compressed data in bytes
  int success;            // 1 if successful, 0 if failed
}
```

### ImageInfo

```dart
class ImageInfo {
  int width;              // Image width in pixels
  int height;             // Image height in pixels
  int bands;              // Number of color bands
  int orientation;        // Image orientation
  int needs_resize;       // Whether resizing is recommended
  int new_width;          // Recommended new width
  int new_height;         // Recommended new height
}
```

## Best Practices

### 1. Memory Management

Always free the compressed buffer when you're done with it:

```dart
final result = compressImage('/path/to/image.jpg', 80);
try {
  if (isCompressionSuccessful(result)) {
    final bytes = compressedResultToBytes(result);
    // Use the bytes...
  }
} finally {
  freeCompressedBuffer(result.data); // Always free!
}
```

### 2. Quality Settings

- **90-100**: High quality, minimal compression
- **70-89**: Good quality, moderate compression
- **50-69**: Acceptable quality, significant compression
- **30-49**: Low quality, high compression
- **1-29**: Very low quality, maximum compression

### 3. Function Selection

- **Basic compression**: Use `compressImage()` for simple needs
- **Size constraints**: Use `compressImageWithSize()` for specific dimensions
- **Large images**: Use `compressLargeImage()` for files > 5MB
- **DSLR photos**: Use `compressLargeDslrImage()` for professional photos
- **Target file size**: Use `smartCompressImage()` for specific file size requirements

### 4. Error Handling

```dart
final result = compressImage('/path/to/image.jpg', 80);
if (isCompressionSuccessful(result)) {
  final bytes = compressedResultToBytes(result);
  if (bytes.isNotEmpty) {
    // Use the compressed image
  } else {
    print('Compression succeeded but no data returned');
  }
} else {
  print('Compression failed');
}
```

## Platform Support

- ✅ Android (ARM64, x86_64)
- ✅ iOS (ARM64, x86_64)
- ✅ macOS (ARM64, x86_64)
- ✅ Linux (x86_64)
- ✅ Windows (x86_64)

## Dependencies

- **VIPS**: High-performance image processing library
- **FFI**: Dart foreign function interface
- **pthread**: Thread safety and synchronization

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Support

For issues and questions, please visit the [GitHub repository](https://github.com/bhumit822/thinpic_flutter).

