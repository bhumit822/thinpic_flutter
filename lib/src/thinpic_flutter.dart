//

// ignore_for_file: unused_element

import 'dart:io';

import 'package:flutter/foundation.dart' show debugPrint, compute;
import 'package:path_provider/path_provider.dart' show getTemporaryDirectory;
import 'package:thinpic_flutter/generated/thinpic_flutter_bindings_generated.dart';

import 'package:thinpic_flutter/src/file_types.dart';
import 'package:thinpic_flutter/src/thinpic_flutter_ffi_functions.dart'
    show
        compressImageWithFormat,
        compressImageWithSizeAndFormat,
        compressLargeImageWithFormat,
        compressLargeDslrImageWithFormat,
        smartCompressImageWithFormat,
        autoCompressImage,
        fastWebPCompress,
        isCompressionSuccessful,
        compressedResultToBytes,
        getImageInfo;

// Isolate functions for each compression operation
Future<dynamic> _compressImageIsolate(Map<String, dynamic> params) async {
  return compressImageWithFormat(
    params['imagePath'] as String,
    params['quality'] as int,
    params['format'] as ImageFormat,
  );
}

Future<dynamic> _getImageInfoIsolate(Map<String, dynamic> params) async {
  return getImageInfo(params['imagePath'] as String);
}

Future<dynamic> _compressImageWithSizeAndFormatIsolate(
  Map<String, dynamic> params,
) async {
  return compressImageWithSizeAndFormat(
    params['imagePath'] as String,
    params['quality'] as int,
    params['targetWidth'] as int,
    params['targetHeight'] as int,
    params['format'] as ImageFormat,
  );
}

Future<dynamic> _compressLargeImageWithFormatIsolate(
  Map<String, dynamic> params,
) async {
  return compressLargeImageWithFormat(
    params['imagePath'] as String,
    params['quality'] as int,
    params['format'] as ImageFormat,
  );
}

Future<dynamic> _compressLargeDslrImageWithFormatIsolate(
  Map<String, dynamic> params,
) async {
  return compressLargeDslrImageWithFormat(
    params['imagePath'] as String,
    params['quality'] as int,
    params['format'] as ImageFormat,
  );
}

Future<dynamic> _smartCompressImageWithFormatIsolate(
  Map<String, dynamic> params,
) async {
  return smartCompressImageWithFormat(
    params['imagePath'] as String,
    params['targetKb'] as int,
    params['type'] as int,
    params['format'] as ImageFormat,
  );
}

Future<dynamic> _autoCompressImageIsolate(Map<String, dynamic> params) async {
  return autoCompressImage(
    params['imagePath'] as String,
    params['quality'] as int,
  );
}

Future<dynamic> _fastWebPCompressIsolate(Map<String, dynamic> params) async {
  return fastWebPCompress(
    params['imagePath'] as String,
    params['quality'] as int,
  );
}

class ThinPicCompress {
  static Future<ImageInfoData?> getImageInfo(String imagePath) async {
    final result = await compute(_getImageInfoIsolate, {
      'imagePath': imagePath,
    });
    return result;
  }

  /// compress image with format
  ///
  /// [imagePath] - path to the image to compress
  /// [quality] - quality of the compressed image
  /// [format] - format of the compressed image
  ///
  /// Returns a [File] object if compression is successful, otherwise returns null
  /// example:
  /// ```dart
  /// final result = await ThinPicCompress.compressImage(
  ///   'path/to/image.jpg',
  ///   quality: 80,
  ///   format: ImageFormat.FORMAT_PNG,
  /// );
  /// ```
  static Future<File?> compressImage(
    String imagePath, {
    int quality = 80,
    ImageFormat format = ImageFormat.FORMAT_JPEG,
  }) async {
    try {
      final result = await compute(_compressImageIsolate, {
        'imagePath': imagePath,
        'quality': quality,
        'format': format,
      });

      if (isCompressionSuccessful(result)) {
        final bytes = compressedResultToBytes(result);
        debugPrint('Compression successful, bytes length: ${bytes.length}');

        // temp path
        final tempPath = await getTemporaryDirectory();
        // save to temp file with appropriate extension
        final extension = _getFileExtension(format);
        final tempFile = File(
          '${tempPath.path}/ ${DateTime.now().millisecondsSinceEpoch}.$extension',
        );
        await tempFile.writeAsBytes(bytes);
        debugPrint('Compressed image saved to: ${tempFile.path}');

        return tempFile;
      }
    } catch (e, stackTrace) {
      debugPrint('Error during compression: $e');
      debugPrint('Stack trace: $stackTrace');
    }
    return null;
  }

  /// compress image with size and format
  ///
  /// [imagePath] - path to the image to compress
  /// [quality] - quality of the compressed image
  /// [targetWidth] - target width of the compressed image
  /// [targetHeight] - target height of the compressed image
  /// [format] - format of the compressed image
  ///  example:
  /// ```dart
  /// final result = await ThinPicCompress.compressImageWithSizeAndFormat(
  ///   'path/to/image.jpg',
  ///   quality: 80,
  ///   targetWidth: 1024,
  ///   targetHeight: 1024,
  ///   format: ImageFormat.FORMAT_PNG,
  /// );
  /// ```
  static Future<File?> compressImageWithSizeAndFormat(
    String imagePath,
    int quality,
    int targetWidth,
    int targetHeight,
    ImageFormat format,
  ) async {
    try {
      final result = await compute(_compressImageWithSizeAndFormatIsolate, {
        'imagePath': imagePath,
        'quality': quality,
        'targetWidth': targetWidth,
        'targetHeight': targetHeight,
        'format': format,
      });

      if (isCompressionSuccessful(result)) {
        final bytes = compressedResultToBytes(result);
        debugPrint('Compression successful, bytes length: ${bytes.length}');

        // temp path
        final tempPath = await getTemporaryDirectory();
        // save to temp file with appropriate extension
        final extension = _getFileExtension(format);
        final tempFile = File(
          '${tempPath.path}/ ${DateTime.now().millisecondsSinceEpoch}.$extension',
        );
        await tempFile.writeAsBytes(bytes);
        debugPrint('Compressed image saved to: ${tempFile.path}');

        return tempFile;
      }
    } catch (e, stackTrace) {
      debugPrint('Error during compression: $e');
      debugPrint('Stack trace: $stackTrace');
    }
    return null;
  }

  static Future<File?> _compressLargeImageWithFormat(
    String imagePath,
    int quality,
    ImageFormat format,
  ) async {
    try {
      final result = await compute(_compressLargeImageWithFormatIsolate, {
        'imagePath': imagePath,
        'quality': quality,
        'format': format,
      });

      if (isCompressionSuccessful(result)) {
        final bytes = compressedResultToBytes(result);
        debugPrint('Compression successful, bytes length: ${bytes.length}');

        // temp path
        final tempPath = await getTemporaryDirectory();
        // save to temp file with appropriate extension
        final extension = _getFileExtension(format);
        final tempFile = File(
          '${tempPath.path}/ ${DateTime.now().millisecondsSinceEpoch}.$extension',
        );
        await tempFile.writeAsBytes(bytes);
        debugPrint('Compressed image saved to: ${tempFile.path}');

        return tempFile;
      }
    } catch (e, stackTrace) {
      debugPrint('Error during compression: $e');
      debugPrint('Stack trace: $stackTrace');
    }
    return null;
  }

  static Future<File?> _compressLargeDslrImageWithFormat(
    String imagePath,
    int quality,
    ImageFormat format,
  ) async {
    try {
      final result = await compute(_compressLargeDslrImageWithFormatIsolate, {
        'imagePath': imagePath,
        'quality': quality,
        'format': format,
      });

      if (isCompressionSuccessful(result)) {
        final bytes = compressedResultToBytes(result);
        debugPrint('Compression successful, bytes length: ${bytes.length}');

        // temp path
        final tempPath = await getTemporaryDirectory();
        // save to temp file with appropriate extension
        final extension = _getFileExtension(format);
        final tempFile = File(
          '${tempPath.path}/ ${DateTime.now().millisecondsSinceEpoch}.$extension',
        );
        await tempFile.writeAsBytes(bytes);
        debugPrint('Compressed image saved to: ${tempFile.path}');

        return tempFile;
      }
    } catch (e, stackTrace) {
      debugPrint('Error during compression: $e');
      debugPrint('Stack trace: $stackTrace');
    }
    return null;
  }

  static Future<File?> _smartCompressImageWithFormat(
    String imagePath,
    int targetKb,
    int type,
    ImageFormat format,
  ) async {
    try {
      final result = await compute(_smartCompressImageWithFormatIsolate, {
        'imagePath': imagePath,
        'targetKb': targetKb,
        'type': type,
        'format': format,
      });

      if (isCompressionSuccessful(result)) {
        final bytes = compressedResultToBytes(result);
        debugPrint('Compression successful, bytes length: ${bytes.length}');

        // temp path
        final tempPath = await getTemporaryDirectory();
        // save to temp file with appropriate extension
        final extension = _getFileExtension(format);
        final tempFile = File(
          '${tempPath.path}/ ${DateTime.now().millisecondsSinceEpoch}.$extension',
        );
        await tempFile.writeAsBytes(bytes);
        debugPrint('Compressed image saved to: ${tempFile.path}');

        return tempFile;
      }
    } catch (e, stackTrace) {
      debugPrint('Error during compression: $e');
      debugPrint('Stack trace: $stackTrace');
    }
    return null;
  }

  static Future<File?> _autoCompressImage(
    String imagePath, {
    int quality = 80,
  }) async {
    try {
      final result = await compute(_autoCompressImageIsolate, {
        'imagePath': imagePath,
        'quality': quality,
      });

      if (isCompressionSuccessful(result)) {
        final bytes = compressedResultToBytes(result);
        debugPrint(
          'Auto-compression successful, bytes length: ${bytes.length}',
        );

        // temp path
        final tempPath = await getTemporaryDirectory();
        // save to temp file with auto-detected extension
        final tempFile = File(
          '${tempPath.path}/ ${DateTime.now().millisecondsSinceEpoch}_auto',
        );
        await tempFile.writeAsBytes(bytes);
        debugPrint('Auto-compressed image saved to: ${tempFile.path}');

        return tempFile;
      }
    } catch (e, stackTrace) {
      debugPrint('Error during auto-compression: $e');
      debugPrint('Stack trace: $stackTrace');
    }
    return null;
  }

  static Future<File?> _fastWebPCompress(
    String imagePath, {
    int quality = 80,
  }) async {
    try {
      final result = await compute(_fastWebPCompressIsolate, {
        'imagePath': imagePath,
        'quality': quality,
      });

      if (isCompressionSuccessful(result)) {
        final bytes = compressedResultToBytes(result);
        debugPrint(
          'Fast WebP compression successful, bytes length: ${bytes.length}',
        );

        // temp path
        final tempPath = await getTemporaryDirectory();
        // save to temp file
        final tempFile = File(
          '${tempPath.path}/ ${DateTime.now().millisecondsSinceEpoch}_fast.webp',
        );
        await tempFile.writeAsBytes(bytes);
        debugPrint('Fast WebP compressed image saved to: ${tempFile.path}');

        return tempFile;
      }
    } catch (e, stackTrace) {
      debugPrint('Error during fast WebP compression: $e');
      debugPrint('Stack trace: $stackTrace');
    }
    return null;
  }

  // Helper function to get file extension based on format
  static String _getFileExtension(ImageFormat format) {
    switch (format) {
      case ImageFormat.FORMAT_JPEG:
        return 'jpg';
      // case ImageFormat.FORMAT_PNG:
      //   return 'png';
      case ImageFormat.FORMAT_WEBP:
        return 'webp';
      // case ImageFormat.FORMAT_TIFF:
      //   return 'tiff';
      // case ImageFormat.FORMAT_HEIF:
      //   return 'heif';
      // case ImageFormat.FORMAT_JP2K:
      //   return 'jp2';
      // case ImageFormat.FORMAT_JXL:
      //   return 'jxl';
      // case ImageFormat.FORMAT_GIF:
      // return 'gif';
      case ImageFormat.FORMAT_AUTO:
        return 'jpg'; // Default to jpg for auto
    }
  }
}
