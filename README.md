# thinpic_flutter

A new Flutter FFI plugin project.

## Getting Started

This project is a starting point for a Flutter
[FFI plugin](https://flutter.dev/to/ffi-package),
a specialized package that includes native code directly invoked with Dart FFI.

## Features

### Image Compression Functions

- **compressImage(inputPath, quality)**: Basic image compression with quality control
- **compressImageWithSize(inputPath, quality, width, height)**: Advanced compression with optional size parameters
  - If both width and height are provided, the smallest dimension is used to maintain aspect ratio
  - If only width is provided, height is calculated to maintain aspect ratio
  - If only height is provided, width is calculated to maintain aspect ratio
  - If both are 0 or negative, no resizing is applied (same as basic compression)
- **compressLargeImage(inputPath, quality)**: Optimized for large images
- **compressLargeDslrImage(inputPath, quality)**: Optimized for DSLR images
- **smartCompressImage(inputPath, targetKb, type)**: Smart compression targeting specific file size

### Usage Examples

```dart
import 'package:thinpic_flutter/thinpic_flutter.dart';

// Basic compression
final result = compressImage('/path/to/image.jpg', 80);

// Compression with size constraints
final result1 = compressImageWithSize('/path/to/image.jpg', 80, 1920, 0); // Width only
final result2 = compressImageWithSize('/path/to/image.jpg', 80, 0, 1080); // Height only
final result3 = compressImageWithSize('/path/to/image.jpg', 80, 1920, 1080); // Both dimensions

// Check if compression was successful
if (isCompressionSuccessful(result)) {
  final bytes = compressedResultToBytes(result);
  // Use the compressed image bytes
  freeCompressedBuffer(result.data); // Don't forget to free the buffer
}
```

## Project structure

This template uses the following structure:

* `src`: Contains the native source code, and a CmakeFile.txt file for building
  that source code into a dynamic library.

* `lib`: Contains the Dart code that defines the API of the plugin, and which
  calls into the native code using `dart:ffi`.

* platform folders (`android`, `ios`, `windows`, etc.): Contains the build files
  for building and bundling the native code library with the platform application.

## Building and bundling native code

The `pubspec.yaml` specifies FFI plugins as follows:

```yaml
  plugin:
    platforms:
      some_platform:
        ffiPlugin: true
```

This configuration invokes the native build for the various target platforms
and bundles the binaries in Flutter applications using these FFI plugins.

This can be combined with dartPluginClass, such as when FFI is used for the
implementation of one platform in a federated plugin:

```yaml
  plugin:
    implements: some_other_plugin
    platforms:
      some_platform:
        dartPluginClass: SomeClass
        ffiPlugin: true
```

A plugin can have both FFI and method channels:

```yaml
  plugin:
    platforms:
      some_platform:
        pluginClass: SomeName
        ffiPlugin: true
```

The native build systems that are invoked by FFI (and method channel) plugins are:

* For Android: Gradle, which invokes the Android NDK for native builds.
  * See the documentation in android/build.gradle.
* For iOS and MacOS: Xcode, via CocoaPods.
  * See the documentation in ios/thinpic_flutter.podspec.
  * See the documentation in macos/thinpic_flutter.podspec.
* For Linux and Windows: CMake.
  * See the documentation in linux/CMakeLists.txt.
  * See the documentation in windows/CMakeLists.txt.

## Binding to native code

To use the native code, bindings in Dart are needed.
To avoid writing these by hand, they are generated from the header file
(`src/thinpic_flutter.h`) by `package:ffigen`.
Regenerate the bindings by running `dart run ffigen --config ffigen.yaml`.

## Invoking native code

Very short-running native functions can be directly invoked from any isolate.
For example, see `sum` in `lib/thinpic_flutter.dart`.

Longer-running functions should be invoked on a helper isolate to avoid
dropping frames in Flutter applications.
For example, see `sumAsync` in `lib/thinpic_flutter.dart`.

## Flutter help

For help getting started with Flutter, view our
[online documentation](https://docs.flutter.dev), which offers tutorials,
samples, guidance on mobile development, and a full API reference.

