import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'thinpic_flutter_bindings_generated.dart';

const String _libName = 'thinpic_flutter';

final DynamicLibrary _dylib = () {
  if (Platform.isMacOS || Platform.isIOS) {
    return DynamicLibrary.open('$_libName.framework/$_libName');
  }
  if (Platform.isAndroid || Platform.isLinux) {
    return DynamicLibrary.open('lib$_libName.so');
  }
  if (Platform.isWindows) {
    return DynamicLibrary.open('$_libName.dll');
  }
  throw UnsupportedError('Unknown platform: ${Platform.operatingSystem}');
}();

final ThinpicFlutterBindings _bindings = ThinpicFlutterBindings(_dylib);

CompressedImageResult compressImage(String inputPath, int quality) {
  final inputPathPtr = inputPath.toNativeUtf8();
  try {
    return _bindings.compress_image(inputPathPtr.cast<Char>(), quality);
  } finally {
    malloc.free(inputPathPtr);
  }
}

CompressedImageResult compressImageWithSize(
  String inputPath,
  int quality,
  int targetWidth,
  int targetHeight,
) {
  final inputPathPtr = inputPath.toNativeUtf8();
  try {
    return _bindings.compress_image_with_size(
      inputPathPtr.cast<Char>(),
      quality,
      targetWidth,
      targetHeight,
    );
  } finally {
    malloc.free(inputPathPtr);
  }
}

CompressedImageResult compressLargeImage(String inputPath, int quality) {
  final inputPathPtr = inputPath.toNativeUtf8();
  try {
    return _bindings.compress_large_image(inputPathPtr.cast<Char>(), quality);
  } finally {
    malloc.free(inputPathPtr);
  }
}

CompressedImageResult compressLargeDslrImage(String inputPath, int quality) {
  final inputPathPtr = inputPath.toNativeUtf8();
  try {
    return _bindings.compress_large_dslr_image(
      inputPathPtr.cast<Char>(),
      quality,
    );
  } finally {
    malloc.free(inputPathPtr);
  }
}

CompressedImageResult smartCompressImage(
  String inputPath,
  int targetKb,
  int type,
) {
  final inputPathPtr = inputPath.toNativeUtf8();
  try {
    return _bindings.smart_compress_image(
      inputPathPtr.cast<Char>(),
      targetKb,
      type,
    );
  } finally {
    malloc.free(inputPathPtr);
  }
}

ImageInfo getImageInfo(String inputPath) {
  final inputPathPtr = inputPath.toNativeUtf8();
  try {
    return _bindings.get_image_info(inputPathPtr.cast<Char>());
  } finally {
    malloc.free(inputPathPtr);
  }
}

void freeCompressedBuffer(Pointer<Uint8> buffer) {
  _bindings.free_compressed_buffer(buffer);
}

void shutdownVips() => _bindings.shutdown_vips();

int testVipsBasic() => _bindings.test_vips_basic();

Uint8List compressedResultToBytes(CompressedImageResult result) {
  if (result.success == 0 || result.data == nullptr) {
    return Uint8List(0);
  }
  final bytes = result.data.asTypedList(result.length);
  return Uint8List.fromList(bytes);
}

bool isCompressionSuccessful(CompressedImageResult result) {
  return result.success == 1;
}

(int width, int height) getImageDimensions(ImageInfo info) {
  return (info.width, info.height);
}
