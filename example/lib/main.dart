import 'package:flutter/material.dart';
import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

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

  @override
  void initState() {
    super.initState();
    // Test basic functionality
    testResult = thinpic_flutter.testVipsBasic();
  }

  Future<void> pickImage() async {
    // In a real app, you would use image_picker or file_picker
    // For this example, we'll simulate with a placeholder
    setState(() {
      selectedImagePath = '/path/to/sample/image.jpg';
      compressionStatus =
          'Image selected: ${selectedImagePath!.split('/').last}';
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

  Future<void> compressImage(int quality) async {
    if (selectedImagePath == null) return;

    setState(() {
      isCompressing = true;
      compressionStatus = 'Compressing with quality: $quality...';
    });

    try {
      final result = thinpic_flutter.compressImage(selectedImagePath!, quality);

      if (thinpic_flutter.isCompressionSuccessful(result)) {
        final bytes = thinpic_flutter.compressedResultToBytes(result);
        setState(() {
          compressedImageData = bytes;
          compressionStatus =
              'Compression successful! Size: ${bytes.length} bytes';
        });

        // Free the buffer
        thinpic_flutter.freeCompressedBuffer(result.data);
      } else {
        setState(() {
          compressionStatus = 'Compression failed';
        });
      }
    } catch (e) {
      setState(() {
        compressionStatus = 'Error during compression: $e';
      });
    } finally {
      setState(() {
        isCompressing = false;
      });
    }
  }

  Future<void> smartCompressImage(int targetKb, int type) async {
    if (selectedImagePath == null) return;

    setState(() {
      isCompressing = true;
      compressionStatus = 'Smart compressing to ${targetKb}KB...';
    });

    try {
      final result = thinpic_flutter.smartCompressImage(
        selectedImagePath!,
        targetKb,
        type,
      );

      if (thinpic_flutter.isCompressionSuccessful(result)) {
        final bytes = thinpic_flutter.compressedResultToBytes(result);
        setState(() {
          compressedImageData = bytes;
          compressionStatus =
              'Smart compression successful! Size: ${bytes.length} bytes';
        });

        // Free the buffer
        thinpic_flutter.freeCompressedBuffer(result.data);
      } else {
        setState(() {
          compressionStatus = 'Smart compression failed';
        });
      }
    } catch (e) {
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
    if (selectedImagePath == null) return;

    setState(() {
      isCompressing = true;
      compressionStatus = 'Compressing large image with quality: $quality...';
    });

    try {
      final result = thinpic_flutter.compressLargeImage(
        selectedImagePath!,
        quality,
      );

      if (thinpic_flutter.isCompressionSuccessful(result)) {
        final bytes = thinpic_flutter.compressedResultToBytes(result);
        setState(() {
          compressedImageData = bytes;
          compressionStatus =
              'Large image compression successful! Size: ${bytes.length} bytes';
        });

        // Free the buffer
        thinpic_flutter.freeCompressedBuffer(result.data);
      } else {
        setState(() {
          compressionStatus = 'Large image compression failed';
        });
      }
    } catch (e) {
      setState(() {
        compressionStatus = 'Error during large image compression: $e';
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
