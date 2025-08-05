/// Supported image formats
enum ImageFormat {
  FORMAT_JPEG(0),
  // FORMAT_PNG(1),
  FORMAT_WEBP(2),
  // FORMAT_TIFF(3),
  // FORMAT_HEIF(4),

  /// JPEG 2000
  // FORMAT_JP2K(5),

  /// JPEG XL
  // FORMAT_JXL(6),
  // FORMAT_GIF(7),

  /// Auto-detect based on input file extension
  FORMAT_AUTO(8);

  final int value;
  const ImageFormat(this.value);

  static ImageFormat fromValue(int value) => switch (value) {
    0 => FORMAT_JPEG,
    // 1 => FORMAT_PNG,
    2 => FORMAT_WEBP,
    // 3 => FORMAT_TIFF,
    // 4 => FORMAT_HEIF,
    // 5 => FORMAT_JP2K,
    // 6 => FORMAT_JXL,
    // 7 => FORMAT_GIF,
    8 => FORMAT_AUTO,
    _ => throw ArgumentError("Unknown value for ImageFormat: $value"),
  };
}
