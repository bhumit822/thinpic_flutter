// lib/image_compressor_bindings.dart

import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'dart:io';
import 'dart:typed_data';

final DynamicLibrary _lib = Platform.isAndroid
    ? DynamicLibrary.open("libimagecompressor.so")
    : DynamicLibrary.process(); // For unit test on desktop

typedef _CompressNative = Int32 Function(Pointer<Utf8>, Pointer<Utf8>, Int32);
typedef _CompressDart = int Function(Pointer<Utf8>, Pointer<Utf8>, int);

typedef _TestVipsNative = Int32 Function();
typedef _TestVipsDart = int Function();

typedef _TestImageLoadingNative = Int32 Function(Pointer<Utf8>);
typedef _TestImageLoadingDart = int Function(Pointer<Utf8>);

typedef _TestImageSavingNative = Int32 Function(Pointer<Utf8>);
typedef _TestImageSavingDart = int Function(Pointer<Utf8>);

typedef _TestVipsBasicNative = Int32 Function();
typedef _TestVipsBasicDart = int Function();

// Structure for compressed image result
final class CompressedImageResult extends Struct {
  external Pointer<Uint8> data;
  @Uint64()
  external int length;
  @Int32()
  external int success;
}

// Structure for image information
final class ImageInfo extends Struct {
  @Int32()
  external int width;
  @Int32()
  external int height;
  @Int32()
  external int bands;
  @Int32()
  external int orientation;
  @Int32()
  external int needs_resize;
  @Int32()
  external int new_width;
  @Int32()
  external int new_height;
}

typedef _CompressImageNative =
    CompressedImageResult Function(Pointer<Utf8>, Int32);
typedef _CompressImageDart = CompressedImageResult Function(Pointer<Utf8>, int);

typedef _SmartCompressImageNative =
    CompressedImageResult Function(Pointer<Utf8>, Int32, Int32);
typedef _SmartCompressImageDart =
    CompressedImageResult Function(Pointer<Utf8>, int, int);

typedef _GetImageInfoNative = ImageInfo Function(Pointer<Utf8>);
typedef _GetImageInfoDart = ImageInfo Function(Pointer<Utf8>);

typedef _FreeBufferNative = Void Function(Pointer<Uint8>);
typedef _FreeBufferDart = void Function(Pointer<Uint8>);

typedef _ShutdownVipsNative = Void Function();
typedef _ShutdownVipsDart = void Function();

final _CompressImageDart compressImage = _lib
    .lookupFunction<_CompressImageNative, _CompressImageDart>("compress_image");

final _SmartCompressImageDart smartCompressImage = _lib
    .lookupFunction<_SmartCompressImageNative, _SmartCompressImageDart>(
      "smart_compress_image",
    );

final _TestVipsDart testVipsInitialization = _lib
    .lookupFunction<_TestVipsNative, _TestVipsDart>("test_vips_initialization");

final _TestImageLoadingDart testImageLoading = _lib
    .lookupFunction<_TestImageLoadingNative, _TestImageLoadingDart>(
      "test_image_loading",
    );

final _TestImageSavingDart testImageSaving = _lib
    .lookupFunction<_TestImageSavingNative, _TestImageSavingDart>(
      "test_image_saving",
    );

final _GetImageInfoDart getImageInfo = _lib
    .lookupFunction<_GetImageInfoNative, _GetImageInfoDart>("get_image_info");

final _FreeBufferDart freeCompressedBuffer = _lib
    .lookupFunction<_FreeBufferNative, _FreeBufferDart>(
      "free_compressed_buffer",
    );

final _ShutdownVipsDart shutdownVips = _lib
    .lookupFunction<_ShutdownVipsNative, _ShutdownVipsDart>("shutdown_vips");

final _TestVipsBasicDart testVipsBasic = _lib
    .lookupFunction<_TestVipsBasicNative, _TestVipsBasicDart>(
      "test_vips_basic",
    );

// Test function to verify VIPS is working
Future<bool> testVips() async {
  try {
    final result = testVipsInitialization();
    return result == 0;
  } catch (e) {
    print('VIPS test failed: $e');
    return false;
  }
}

// Test function to verify basic VIPS functionality
Future<bool> testVipsBasicFunction() async {
  try {
    final result = testVipsBasic();
    return result == 0;
  } catch (e) {
    print('Basic VIPS test failed: $e');
    return false;
  }
}

// Test function to verify image loading
Future<bool> testImageLoadingDart(String imagePath) async {
  try {
    final pathPtr = imagePath.toNativeUtf8();
    final result = testImageLoading(pathPtr);
    malloc.free(pathPtr);
    return result == 0;
  } catch (e) {
    print('Image loading test failed: $e');
    return false;
  }
}

// Test function to verify image saving
Future<bool> testImageSavingDart(String imagePath) async {
  try {
    final pathPtr = imagePath.toNativeUtf8();
    final result = testImageSaving(pathPtr);
    malloc.free(pathPtr);
    return result == 0;
  } catch (e) {
    print('Image saving test failed: $e');
    return false;
  }
}

// Function to compress image and return buffer
Future<Uint8List?> compressImageToBuffer(String inputPath, int quality) async {
  try {
    // Input validation
    if (inputPath.isEmpty) {
      throw ArgumentError('Input path cannot be empty');
    }

    if (quality < 1 || quality > 100) {
      throw ArgumentError('Quality must be between 1 and 100');
    }

    // Check if input file exists
    final inputFile = File(inputPath);
    if (!await inputFile.exists()) {
      throw FileSystemException('Input file does not exist', inputPath);
    }

    final pathPtr = inputPath.toNativeUtf8();

    try {
      final result = compressImage(pathPtr, quality);

      if (result.success == 0 && result.data != nullptr && result.length > 0) {
        // Copy the data to Dart-owned memory
        final buffer = result.data.asTypedList(result.length);
        final dartBuffer = Uint8List.fromList(buffer);

        // Free the native buffer
        freeCompressedBuffer(result.data);

        return dartBuffer;
      } else {
        // Free the buffer if it exists but failed
        if (result.data != nullptr) {
          freeCompressedBuffer(result.data);
        }
        throw Exception(
          'Compression failed with success code: ${result.success}',
        );
      }
    } finally {
      malloc.free(pathPtr);
    }
  } catch (e) {
    print('Image compression failed: $e');
    rethrow;
  }
}

// Function to smart compress image to target size and return buffer
Future<Uint8List?> smartCompressImageToBuffer(
  String inputPath,
  int targetKb,
  int type,
) async {
  try {
    // Input validation
    if (inputPath.isEmpty) {
      throw ArgumentError('Input path cannot be empty');
    }

    if (targetKb <= 0) {
      throw ArgumentError('Target KB must be greater than 0');
    }

    if (type != 0 && type != 1) {
      throw ArgumentError('Type must be 0 (low) or 1 (high)');
    }

    // Check if input file exists
    final inputFile = File(inputPath);
    if (!await inputFile.exists()) {
      throw FileSystemException('Input file does not exist', inputPath);
    }

    final pathPtr = inputPath.toNativeUtf8();

    try {
      final result = smartCompressImage(pathPtr, targetKb, type);

      if (result.success == 0 && result.data != nullptr && result.length > 0) {
        // Copy the data to Dart-owned memory
        final buffer = result.data.asTypedList(result.length);
        final dartBuffer = Uint8List.fromList(buffer);

        // Free the native buffer
        freeCompressedBuffer(result.data);

        return dartBuffer;
      } else {
        // Free the buffer if it exists but failed
        if (result.data != nullptr) {
          freeCompressedBuffer(result.data);
        }
        throw Exception(
          'Smart compression failed with success code: ${result.success}',
        );
      }
    } finally {
      malloc.free(pathPtr);
    }
  } catch (e) {
    print('Smart image compression failed: $e');
    rethrow;
  }
}

// Function to compress image and save directly to file
Future<bool> compressImageToFile(
  String inputPath,
  String outputPath,
  int quality,
) async {
  try {
    // Input validation
    if (inputPath.isEmpty || outputPath.isEmpty) {
      throw ArgumentError('Input and output paths cannot be empty');
    }

    if (quality < 1 || quality > 100) {
      throw ArgumentError('Quality must be between 1 and 100');
    }

    // Check if input file exists
    final inputFile = File(inputPath);
    if (!await inputFile.exists()) {
      throw FileSystemException('Input file does not exist', inputPath);
    }

    // Ensure output directory exists
    final outputFile = File(outputPath);
    final outputDir = outputFile.parent;
    if (!await outputDir.exists()) {
      await outputDir.create(recursive: true);
    }

    final pathPtr = inputPath.toNativeUtf8();

    try {
      final result = compressImage(pathPtr, quality);

      if (result.success == 0 && result.data != nullptr && result.length > 0) {
        // Copy the data to Dart-owned memory
        final buffer = result.data.asTypedList(result.length);
        final dartBuffer = Uint8List.fromList(buffer);

        // Free the native buffer
        freeCompressedBuffer(result.data);

        // Write to file
        await outputFile.writeAsBytes(dartBuffer);
        return true;
      } else {
        // Free the buffer if it exists but failed
        if (result.data != nullptr) {
          freeCompressedBuffer(result.data);
        }
        throw Exception(
          'Compression failed with success code: ${result.success}',
        );
      }
    } finally {
      malloc.free(pathPtr);
    }
  } catch (e) {
    print('Image compression to file failed: $e');
    rethrow;
  }
}

// Structure for batch compression result
class BatchCompressionResult {
  final String inputPath;
  final String? outputPath;
  final Uint8List? compressedData;
  final bool success;
  final String? error;

  BatchCompressionResult({
    required this.inputPath,
    this.outputPath,
    this.compressedData,
    required this.success,
    this.error,
  });
}

// Function to compress a single image in batch (for concurrent operations)
Future<BatchCompressionResult> compressImageInBatch(
  String inputPath,
  int quality, {
  String? outputPath,
}) async {
  try {
    final compressedBytes = await compressImageToBuffer(inputPath, quality);

    if (compressedBytes != null) {
      // If output path is provided, save to file
      if (outputPath != null) {
        final outputFile = File(outputPath);
        final outputDir = outputFile.parent;
        if (!await outputDir.exists()) {
          await outputDir.create(recursive: true);
        }
        await outputFile.writeAsBytes(compressedBytes);
      }

      return BatchCompressionResult(
        inputPath: inputPath,
        outputPath: outputPath,
        compressedData: compressedBytes,
        success: true,
      );
    } else {
      return BatchCompressionResult(
        inputPath: inputPath,
        outputPath: outputPath,
        success: false,
        error: 'Compression returned null data',
      );
    }
  } catch (e) {
    return BatchCompressionResult(
      inputPath: inputPath,
      outputPath: outputPath,
      success: false,
      error: e.toString(),
    );
  }
}

// Function to compress multiple images in batches with concurrency control
Future<List<BatchCompressionResult>> compressImagesInBatches(
  List<String> inputPaths,
  int quality, {
  List<String>? outputPaths,
  int batchSize = 10,
  int maxConcurrency = 4,
}) async {
  final results = <BatchCompressionResult>[];

  // Process images in batches
  for (int i = 0; i < inputPaths.length; i += batchSize) {
    final batchEnd = (i + batchSize < inputPaths.length)
        ? i + batchSize
        : inputPaths.length;
    final batchInputs = inputPaths.sublist(i, batchEnd);
    final batchOutputs = outputPaths?.sublist(i, batchEnd);

    print(
      'Processing batch ${(i ~/ batchSize) + 1}: ${batchInputs.length} images',
    );

    // Process batch with concurrency control
    final batchResults = await Future.wait(
      batchInputs.asMap().entries.map((entry) {
        final index = entry.key;
        final inputPath = entry.value;
        final outputPath = batchOutputs?[index];

        return compressImageInBatch(inputPath, quality, outputPath: outputPath);
      }),
    );

    results.addAll(batchResults);

    // Small delay between batches to prevent overwhelming the system
    if (batchEnd < inputPaths.length) {
      await Future.delayed(Duration(milliseconds: 100));
    }
  }

  return results;
}

// Function to get compression statistics from batch results
Map<String, dynamic> getBatchCompressionStats(
  List<BatchCompressionResult> results,
) {
  final total = results.length;
  final successful = results.where((r) => r.success).length;
  final failed = total - successful;

  return {
    'total': total,
    'successful': successful,
    'failed': failed,
    'successRate': total > 0
        ? (successful / total * 100).toStringAsFixed(1) + '%'
        : '0%',
  };
}

// Function to get image information
Future<Map<String, dynamic>?> getImageInfoDart(String inputPath) async {
  try {
    // Input validation
    if (inputPath.isEmpty) {
      throw ArgumentError('Input path cannot be empty');
    }

    // Check if input file exists
    final inputFile = File(inputPath);
    if (!await inputFile.exists()) {
      throw FileSystemException('Input file does not exist', inputPath);
    }

    final pathPtr = inputPath.toNativeUtf8();

    try {
      final info = getImageInfo(pathPtr);

      return {
        'width': info.width,
        'height': info.height,
        'bands': info.bands,
        'orientation': info.orientation,
        'needs_resize': info.needs_resize == 1,
        'new_width': info.new_width,
        'new_height': info.new_height,
        'aspect_ratio': info.width > 0
            ? (info.height / info.width).toStringAsFixed(3)
            : '0',
        'orientation_description': _getOrientationDescription(info.orientation),
      };
    } finally {
      malloc.free(pathPtr);
    }
  } catch (e) {
    print('Get image info failed: $e');
    rethrow;
  }
}

// Helper function to get orientation description
String _getOrientationDescription(int orientation) {
  switch (orientation) {
    case 1:
      return 'Normal (0°)';
    case 2:
      return 'Mirror horizontal';
    case 3:
      return 'Rotate 180°';
    case 4:
      return 'Mirror vertical';
    case 5:
      return 'Mirror horizontal and rotate 270° CW';
    case 6:
      return 'Rotate 90° CW';
    case 7:
      return 'Mirror horizontal and rotate 90° CW';
    case 8:
      return 'Rotate 270° CW';
    default:
      return 'Unknown';
  }
}
