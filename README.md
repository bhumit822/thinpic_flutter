# thinpic_flutter

A high-performance Flutter FFI plugin for image compression using the VIPS image processing library. This plugin provides efficient image compression with various optimization strategies for different use cases and supports multiple image formats.

<div align="center">
  <img src="sample/sample.gif" alt="ThinPic Flutter Demo" width="300">
</div>

## Features

- **Multiple Image Formats**: Support for JPEG, WebP, and AUTO format detection
- **Cross-Platform**: Works on Android and iOS (in roadmap)
- **High Performance**: Native C implementation using VIPS library
- **Memory Efficient**: Automatic buffer management and cleanup
- **Thread-Safe**: Thread-safe operations with proper synchronization
- **Quality Control**: Precise quality settings from 1-100

## Installation

Add this to your `pubspec.yaml`:

```yaml
dependencies:
  thinpic_flutter: ^0.0.1
```

## Supported Image Formats

The plugin supports the following image formats:

- **JPEG** (`FORMAT_JPEG`): Standard JPEG compression
- **WebP** (`FORMAT_WEBP`): Modern WebP format with excellent compression
- **AUTO** (`FORMAT_AUTO`): Automatic format detection based on file extension

## Platform Support

- ✅ **Android** (ARM64, x86_64)
- 🚧 **iOS** (ARM64, x86_64) - In roadmap, working on it

## API Reference



#### `ThinPicCompress.getImageInfo(String imagePath)`

Retrieves detailed information about an image without compression.

**Parameters:**
- `imagePath` (String): Path to the input image file

**Returns:** `Future<ImageInfoData?>`

**Example:**
```dart
import 'package:thinpic_flutter/thinpic_flutter.dart';

final imageInfo = await ThinPicCompress.getImageInfo('/path/to/image.jpg');
if (imageInfo != null) {
  print('Image dimensions: ${imageInfo.width}x${imageInfo.height}');
}
```

#### `ThinPicCompress.compressImage(String imagePath, {int quality = 80, ImageFormat format = ImageFormat.FORMAT_JPEG})`

Compresses an image with format support and returns a temporary file.

**Parameters:**
- `imagePath` (String): Path to the input image file
- `quality` (int): Compression quality from 1-100 (default: 80)
- `format` (ImageFormat): Target image format (default: JPEG)

**Returns:** `Future<File?>` - Temporary file with compressed image

**Example:**
```dart
import 'package:thinpic_flutter/thinpic_flutter.dart';

// Basic JPEG compression
final compressedFile = await ThinPicCompress.compressImage(
  '/path/to/image.jpg', 
  quality: 80,
  format: ImageFormat.FORMAT_JPEG
);

// WebP compression
final webpFile = await ThinPicCompress.compressImage(
  '/path/to/image.jpg', 
  quality: 85,
  format: ImageFormat.FORMAT_WEBP
);

if (compressedFile != null) {
  print('Compressed image saved to: ${compressedFile.path}');
}
```

#### `ThinPicCompress.compressImageWithSizeAndFormat(String imagePath, int quality, int targetWidth, int targetHeight, ImageFormat format)`

Compresses an image with size constraints and format support.

**Parameters:**
- `imagePath` (String): Path to the input image file
- `quality` (int): Compression quality from 1-100
- `targetWidth` (int): Target width in pixels
- `targetHeight` (int): Target height in pixels
- `format` (ImageFormat): Target image format

**Returns:** `Future<File?>` - Temporary file with compressed image

**Example:**
```dart
import 'package:thinpic_flutter/thinpic_flutter.dart';

final compressedFile = await ThinPicCompress.compressImageWithSizeAndFormat(
  '/path/to/image.jpg', 
  80, 
  1920, 
  1080, 
  ImageFormat.FORMAT_WEBP
);

if (compressedFile != null) {
  print('Resized and compressed image saved to: ${compressedFile.path}');
}
```

## Best Practices

### 1. Quality Settings

- **90-100**: High quality, minimal compression
- **70-89**: Good quality, moderate compression
- **50-69**: Acceptable quality, significant compression
- **30-49**: Low quality, high compression
- **1-29**: Very low quality, maximum compression

### 2. Format Selection

- **JPEG**: Good for photos, widely supported
- **WebP**: Best for photos and web images (smallest size)
- **AUTO**: Automatic format detection based on file extension

### 3. Error Handling

```dart
final compressedFile = await ThinPicCompress.compressImage('/path/to/image.jpg');
if (compressedFile != null) {
  print('Compression successful: ${compressedFile.path}');
} else {
  print('Compression failed');
}
```

### 4. Size Optimization

#### Understanding File Size

When converting between image formats, file size might increase due to:

1. **Different compression algorithms**: Each format has different compression characteristics
2. **Quality settings**: Different formats interpret quality parameters differently
3. **Metadata**: Some formats preserve more metadata than others

#### Best Practices for Size Reduction

1. **Use WebP for Photos**:
   ```dart
   // WebP typically provides the smallest file size for photos
   final result = await ThinPicCompress.compressImage(
     '/path/to/photo.jpg', 
     quality: 80, 
     format: ImageFormat.FORMAT_WEBP
   );
   ```

2. **Use JPEG for Maximum Compatibility**:
   ```dart
   // JPEG is widely supported across all platforms
   final result = await ThinPicCompress.compressImage(
     '/path/to/image.jpg', 
     quality: 80, 
     format: ImageFormat.FORMAT_JPEG
   );
   ```

3. **Use AUTO for Automatic Selection**:
   ```dart
   // Let the plugin choose the best format based on file extension
   final result = await ThinPicCompress.compressImage(
     '/path/to/image.jpg', 
     quality: 80, 
     format: ImageFormat.FORMAT_AUTO
   );
   ```

## Dependencies

- **VIPS**: High-performance image processing library
- **FFI**: Dart foreign function interface
- **path_provider**: For temporary file management

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Support

For issues and questions, please visit the [GitHub repository](https://github.com/bhumit822/thinpic_flutter).

