import 'package:flutter/material.dart';
import 'dart:async';
import 'dart:io';
import 'dart:typed_data';
import 'package:image_picker/image_picker.dart';
import 'package:open_file/open_file.dart';
import 'package:path_provider/path_provider.dart';
import 'package:thinpic_flutter/thinpic_flutter.dart' as thinpic_flutter;
import 'package:thinpic_flutter/thinpic_flutter_bindings_generated.dart'
    as bindings;

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  late int testResult;
  String? selectedImagePath;
  String compressionStatus = 'No image selected';
  bool isCompressing = false;
  bindings.ImageInfo? imageInfo;
  Uint8List? compressedImageData;
  double? compressedImageSize;

  @override
  void initState() {
    super.initState();
    // Test basic functionality
    testResult = thinpic_flutter.testVipsBasic();
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
    });

    // Get image info
    try {
      final info = thinpic_flutter.getImageInfo(selectedImagePath!);
      setState(() {
        imageInfo = info;
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
      final info = thinpic_flutter.getImageInfo(selectedImagePath!);
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
      final result = thinpic_flutter.compressImage(selectedImagePath!, quality);

      if (thinpic_flutter.isCompressionSuccessful(result)) {
        final bytes = thinpic_flutter.compressedResultToBytes(result);
        print('Compression successful, bytes length: ${bytes.length}');

        // temp path
        final tempPath = await getTemporaryDirectory();
        // save to temp file
        final tempFile = File('${tempPath.path}/compressed_image.jpg');
        await tempFile.writeAsBytes(bytes);
        // open the file
        await OpenFile.open(tempFile.path);
        setState(() {
          compressedImageData = bytes;
          compressionStatus =
              'Compression successful! Size: ${bytes.length} bytes';
          compressedImageSize = bytes.length / 1024;
        });
        // Free the buffer
        thinpic_flutter.freeCompressedBuffer(result.data);
      } else {
        print('Compression failed - result.success = ${result.success}');
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

  // smart compress and save to temp file
  Future<void> smartCompressImage(int targetKb, int type) async {
    if (selectedImagePath == null) {
      setState(() {
        compressionStatus = 'Please select an image first';
      });
      return;
    }

    setState(() {
      isCompressing = true;
      compressionStatus = 'Smart compressing to ${targetKb}KB...';
    });

    try {
      print(
        'Smart compressing image: $selectedImagePath to ${targetKb}KB, type: $type',
      );
      final result = thinpic_flutter.smartCompressImage(
        selectedImagePath!,
        targetKb,
        type,
      );

      if (thinpic_flutter.isCompressionSuccessful(result)) {
        final bytes = thinpic_flutter.compressedResultToBytes(result);
        print('Smart compression successful, bytes length: ${bytes.length}');
        setState(() async {
          compressedImageData = bytes;
          compressionStatus =
              'Smart compression successful! Size: ${bytes.length} bytes';
          compressedImageSize = bytes.length / 1024;
          // temp path
          final tempPath = await getTemporaryDirectory();
          // save to temp file
          final tempFile = File('${tempPath.path}/compressed_image.jpg');
          await tempFile.writeAsBytes(bytes);
          // open the file
          await OpenFile.open(tempFile.path);
        });

        // Free the buffer
        thinpic_flutter.freeCompressedBuffer(result.data);
      } else {
        print('Smart compression failed - result.success = ${result.success}');
        setState(() {
          compressionStatus =
              'Smart compression failed - check if image format is supported';
        });
      }
    } catch (e) {
      print('Error during smart compression: $e');
      setState(() {
        compressionStatus = 'Error during smart compression: $e';
      });
    } finally {
      setState(() {
        isCompressing = false;
      });
    }
  }

  Future<void> compressLargeImage(int quality) async {
    if (selectedImagePath == null) {
      setState(() {
        compressionStatus = 'Please select an image first';
      });
      return;
    }

    setState(() {
      isCompressing = true;
      compressionStatus = 'Compressing large image with quality: $quality...';
    });

    try {
      print(
        'Compressing large image: $selectedImagePath with quality: $quality',
      );
      final result = thinpic_flutter.compressLargeImage(
        selectedImagePath!,
        quality,
      );

      if (thinpic_flutter.isCompressionSuccessful(result)) {
        final bytes = thinpic_flutter.compressedResultToBytes(result);
        print(
          'Large image compression successful, bytes length: ${bytes.length}',
        );
        setState(() async {
          compressedImageData = bytes;
          compressionStatus =
              'Large image compression successful! Size: ${bytes.length} bytes';
          compressedImageSize = bytes.length / 1024;
          // temp path
          final tempPath = await getTemporaryDirectory();
          // save to temp file
          final tempFile = File('${tempPath.path}/compressed_image.jpg');
          await tempFile.writeAsBytes(bytes);
          // open the file
          await OpenFile.open(tempFile.path);
        });

        // Free the buffer
        thinpic_flutter.freeCompressedBuffer(result.data);
      } else {
        print(
          'Large image compression failed - result.success = ${result.success}',
        );
        setState(() {
          compressionStatus =
              'Large image compression failed - check if image format is supported';
        });
      }
    } catch (e) {
      print('Error during large image compression: $e');
      setState(() {
        compressionStatus = 'Error during large image compression: $e';
      });
    } finally {
      setState(() {
        isCompressing = false;
      });
    }
  }

  // Save compressed image to a specific directory with custom filename
  Future<void> saveCompressedImageToDirectory(
    String filename, {
    String? directory,
  }) async {
    if (compressedImageData == null) {
      setState(() {
        compressionStatus =
            'No compressed image available. Please compress an image first.';
      });
      return;
    }

    try {
      // Get the documents directory or use the specified directory
      final Directory targetDir;
      if (directory != null) {
        targetDir = Directory(directory);
        if (!await targetDir.exists()) {
          await targetDir.create(recursive: true);
        }
      } else {
        targetDir = await getApplicationDocumentsDirectory();
      }

      // Create the file path
      final filePath = '${targetDir.path}/$filename';
      final file = File(filePath);

      // open the file
      await OpenFile.open(filePath);

      // Write the compressed image data
      await file.writeAsBytes(compressedImageData!);

      setState(() {
        compressionStatus = 'Image saved successfully to: $filePath';
      });

      // Open the saved file
      await OpenFile.open(filePath);
    } catch (e) {
      print('Error saving compressed image: $e');
      setState(() {
        compressionStatus = 'Error saving image: $e';
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
      final originalInfo = thinpic_flutter.getImageInfo(selectedImagePath!);
      print('Original image: ${originalInfo.width}x${originalInfo.height}');

      // Compress with dimensions (this would need to be implemented in the native library)
      // For now, we'll use the regular compression and show the target dimensions
      final result = thinpic_flutter.compressImageWithSize(
        selectedImagePath!,
        quality,
        targetWidth,
        targetHeight,
      );

      if (thinpic_flutter.isCompressionSuccessful(result)) {
        final bytes = thinpic_flutter.compressedResultToBytes(result);
        print(
          'Dimension compression successful, bytes length: ${bytes.length}',
        );
        // temp path
        final tempPath = await getTemporaryDirectory();
        // save to temp file
        final tempFile = File('${tempPath.path}/compressed_image.jpg');
        await tempFile.writeAsBytes(bytes);
        // open the file
        await OpenFile.open(tempFile.path);

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

        // Free the buffer
        thinpic_flutter.freeCompressedBuffer(result.data);
      } else {
        print(
          'Dimension compression failed - result.success = ${result.success}',
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

                // Test result
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text(
                          'Library Status:',
                          style: TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        Text('testVipsBasic() = $testResult', style: textStyle),
                      ],
                    ),
                  ),
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
                            'Compressed size: ${compressedImageSize?.toStringAsFixed(2)} KB',
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
                                      : () => compressLargeImage(80),
                                  child: const Text('Large Image'),
                                ),
                              ),
                              const SizedBox(width: 8),
                              Expanded(
                                child: ElevatedButton(
                                  onPressed: isCompressing
                                      ? null
                                      : () => smartCompressImage(100, 0),
                                  child: const Text('Smart (100KB)'),
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
                                          4500,
                                          6000,
                                          80,
                                        ),
                                  child: const Text('4500x6000 (80%)'),
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

                // Available functions info
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Text(
                          'Available Functions:',
                          style: TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        const SizedBox(height: 8),
                        const Text(
                          '• compressImage(inputPath, quality)\n'
                          '• compressLargeImage(inputPath, quality)\n'
                          '• compressLargeDslrImage(inputPath, quality)\n'
                          '• smartCompressImage(inputPath, targetKb, type)\n'
                          '• getImageInfo(inputPath)\n'
                          '• freeCompressedBuffer(buffer)\n'
                          '• shutdownVips()\n'
                          '• testVipsBasic()',
                          style: TextStyle(fontSize: 14),
                        ),
                      ],
                    ),
                  ),
                ),

                spacerMedium,

                // Shutdown button
                Center(
                  child: ElevatedButton(
                    onPressed: () {
                      thinpic_flutter.shutdownVips();
                      ScaffoldMessenger.of(context).showSnackBar(
                        const SnackBar(content: Text('Vips shutdown called')),
                      );
                    },
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.red,
                      foregroundColor: Colors.white,
                    ),
                    child: const Text('Shutdown Vips'),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
