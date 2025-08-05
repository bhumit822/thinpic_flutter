import 'package:flutter/material.dart';
import 'dart:async';
import 'dart:io';
import 'dart:typed_data';
import 'package:image_picker/image_picker.dart';
import 'package:open_file/open_file.dart';
import 'package:path_provider/path_provider.dart';
import 'package:thinpic_flutter/thinpic_flutter.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  String? selectedImagePath;
  String compressionStatus = 'No image selected';
  bool isCompressing = false;
  ImageInfoData? imageInfo;
  Uint8List? compressedImageData;
  double? originalImageSize;
  String? originalImageFormat;
  String? compressedImageFormat;
  double? compressedImageSize;
  String? compressedImagePath;

  @override
  void initState() {
    super.initState();
  }

  Future<void> pickImage() async {
    final image = await ImagePicker().pickImage(source: ImageSource.gallery);
    if (image == null) {
      setState(() {
        compressionStatus = 'No image selected';
      });
      return;
    }

    setState(() {
      selectedImagePath = image.path;
      compressionStatus = 'Image selected: ${image.path.split('/').last}';
      imageInfo = null;
      compressedImageData = null;
      originalImageSize = null;
      originalImageFormat = null;
      compressedImageFormat = null;
    });

    // Get image info
    try {
      final info = await ThinPicCompress.getImageInfo(selectedImagePath!);
      setState(() {
        imageInfo = info;

        originalImageSize = File(selectedImagePath!).lengthSync() / 1024;
        originalImageFormat = image.path.split('.').last;
      });
    } catch (e) {
      setState(() {
        compressionStatus = 'Error getting image info: $e';
      });
    }
  }

  Future<void> getImageInfo() async {
    if (selectedImagePath == null) return;

    try {
      final info = await ThinPicCompress.getImageInfo(selectedImagePath!);
      setState(() {
        imageInfo = info;
      });
    } catch (e) {
      setState(() {
        compressionStatus = 'Error getting image info: $e';
      });
    }
  }

  // compress and save to temp file

  Future<void> compressImage(int quality) async {
    if (selectedImagePath == null) {
      setState(() {
        compressionStatus = 'Please select an image first';
      });
      return;
    }

    setState(() {
      isCompressing = true;
      compressionStatus = 'Compressing with quality: $quality...';
    });

    try {
      print('Compressing image: $selectedImagePath with quality: $quality');
      final result = await ThinPicCompress.compressImage(
        selectedImagePath!,
        quality: quality,
        format: ImageFormat.FORMAT_JPEG,
      );

      if (result != null) {
        final bytes = await result.readAsBytes();
        final extension = result.path.split('.').last;
        compressedImageFormat = extension;
        compressedImagePath = result.path;
        await OpenFile.open(result.path);
        setState(() {
          compressedImageData = bytes;
          compressionStatus =
              'Compression successful! Size: ${bytes.length} bytes';
          compressedImageSize = bytes.length / 1024;
        });
      } else {
        print('Compression failed - result.success = ${result?.path}');
        setState(() {
          compressionStatus =
              'Compression failed - check if image format is supported';
        });
      }
    } catch (e) {
      print('Error during compression: $e');
      setState(() {
        compressionStatus = 'Error during compression: $e';
      });
    } finally {
      setState(() {
        isCompressing = false;
      });
    }
  }

  // Compress image to specific dimensions with quality
  Future<void> compressImageWithDimensions(
    int targetWidth,
    int targetHeight,
    int quality,
  ) async {
    if (selectedImagePath == null) {
      setState(() {
        compressionStatus = 'Please select an image first';
      });
      return;
    }

    setState(() {
      isCompressing = true;
      compressionStatus =
          'Compressing to ${targetWidth}x${targetHeight} with quality: $quality...';
    });

    try {
      print(
        'Compressing image: $selectedImagePath to ${targetWidth}x${targetHeight} with quality: $quality',
      );

      // First get the original image info
      final originalInfo = await ThinPicCompress.getImageInfo(
        selectedImagePath!,
      );
      print('Original image: ${originalInfo?.width}x${originalInfo?.height}');

      // Compress with dimensions (this would need to be implemented in the native library)
      // For now, we'll use the regular compression and show the target dimensions
      final result = await ThinPicCompress.compressImageWithSizeAndFormat(
        selectedImagePath!,
        quality,
        targetWidth,
        targetHeight,
        ImageFormat.FORMAT_JPEG,
      );

      if (result != null) {
        final bytes = await result.readAsBytes();
        print(
          'Dimension compression successful, bytes length: ${bytes.length}',
        );
        // open the file
        compressedImagePath = result.path;
        await OpenFile.open(result.path);

        // Calculate compression ratio
        final originalSize = File(selectedImagePath!).lengthSync();
        final compressionRatio = (bytes.length / originalSize * 100)
            .toStringAsFixed(1);

        setState(() {
          compressedImageData = bytes;
          compressionStatus =
              'Compression successful! Target: ${targetWidth}x${targetHeight}, '
              'Size: ${bytes.length} bytes (${compressionRatio}% of original)';
          compressedImageSize = bytes.length / 1024;
        });
      } else {
        print(
          'Dimension compression failed - result.success = ${result?.path}',
        );
        setState(() {
          compressionStatus =
              'Dimension compression failed - check if image format is supported';
        });
      }
    } catch (e) {
      print('Error during dimension compression: $e');
      setState(() {
        compressionStatus = 'Error during dimension compression: $e';
      });
    } finally {
      setState(() {
        isCompressing = false;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    const textStyle = TextStyle(fontSize: 18);
    const spacerSmall = SizedBox(height: 10);
    const spacerMedium = SizedBox(height: 20);

    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('ThinPic Flutter Demo'),
          backgroundColor: Colors.blue,
          foregroundColor: Colors.white,
        ),
        body: SingleChildScrollView(
          child: Container(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'ThinPic Flutter - Image Compression Demo',
                  style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
                  textAlign: TextAlign.center,
                ),
                spacerMedium,

                // Image selection
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text(
                          'Image Selection:',
                          style: TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        spacerSmall,
                        ElevatedButton(
                          onPressed: pickImage,
                          child: const Text('Select Image'),
                        ),
                        spacerSmall,
                        Text('Status: $compressionStatus', style: textStyle),
                      ],
                    ),
                  ),
                ),
                spacerMedium,

                // Image info
                if (imageInfo != null) ...[
                  Card(
                    child: Padding(
                      padding: const EdgeInsets.all(16),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text(
                            'Image Information:',
                            style: TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                          Text(
                            'Width: ${imageInfo!.width}px',
                            style: textStyle,
                          ),
                          Text(
                            'Height: ${imageInfo!.height}px',
                            style: textStyle,
                          ),
                          Text('Bands: ${imageInfo!.bands}', style: textStyle),
                          Text(
                            'Orientation: ${imageInfo!.orientation}',
                            style: textStyle,
                          ),
                          Text(
                            'Needs resize: ${imageInfo!.needs_resize == 1 ? "Yes" : "No"}',
                            style: textStyle,
                          ),
                          Text(
                            'Original size: ${originalImageSize?.toStringAsFixed(2)} KB ($originalImageFormat)',
                            style: textStyle,
                          ),
                          Text(
                            'Compressed size: ${compressedImageSize?.toStringAsFixed(2)} KB ($compressedImageFormat)',
                            style: textStyle,
                          ),
                        ],
                      ),
                    ),
                  ),
                  spacerMedium,
                ],

                // Compression options
                if (selectedImagePath != null) ...[
                  Card(
                    child: Padding(
                      padding: const EdgeInsets.all(16),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          const Text(
                            'Compression Options:',
                            style: TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                          spacerSmall,
                          Row(
                            children: [
                              Expanded(
                                child: ElevatedButton(
                                  onPressed: isCompressing
                                      ? null
                                      : () => compressImage(80),
                                  child: const Text('Compress (80%)'),
                                ),
                              ),
                              const SizedBox(width: 8),
                              Expanded(
                                child: ElevatedButton(
                                  onPressed: isCompressing
                                      ? null
                                      : () => compressImage(60),
                                  child: const Text('Compress (60%)'),
                                ),
                              ),
                            ],
                          ),
                          spacerSmall,

                          Row(
                            children: [
                              Expanded(
                                child: ElevatedButton(
                                  onPressed: isCompressing
                                      ? null
                                      : () => compressImageWithDimensions(
                                          800,
                                          600,
                                          80,
                                        ),
                                  child: const Text('800x600 (80%)'),
                                ),
                              ),
                              const SizedBox(width: 8),
                              Expanded(
                                child: ElevatedButton(
                                  onPressed: isCompressing
                                      ? null
                                      : () => compressImageWithDimensions(
                                          3500,
                                          6000,
                                          80,
                                        ),
                                  child: const Text('3500x6000 (80%)'),
                                ),
                              ),
                            ],
                          ),
                          spacerSmall,
                          Row(
                            children: [
                              Expanded(
                                child: ElevatedButton(
                                  onPressed: isCompressing
                                      ? null
                                      : () => compressImageWithDimensions(
                                          150,
                                          150,
                                          70,
                                        ),
                                  child: const Text('Thumbnail 150x150'),
                                ),
                              ),
                              const SizedBox(width: 8),
                              Expanded(
                                child: ElevatedButton(
                                  onPressed: isCompressing
                                      ? null
                                      : () => compressImageWithDimensions(
                                          400,
                                          300,
                                          75,
                                        ),
                                  child: const Text('400x300 (75%)'),
                                ),
                              ),
                            ],
                          ),
                        ],
                      ),
                    ),
                  ),
                  spacerMedium,
                ],

                spacerMedium,
                // open original image and compressed image
                Row(
                  children: [
                    ElevatedButton(
                      onPressed: () => OpenFile.open(selectedImagePath!),
                      child: const Text('Open Original Image'),
                    ),
                    ElevatedButton(
                      onPressed: () => OpenFile.open(compressedImagePath!),
                      child: const Text('Open Compressed Image'),
                    ),
                  ],
                ),
                // Shutdown button
              ],
            ),
          ),
        ),
      ),
    );
  }
}
