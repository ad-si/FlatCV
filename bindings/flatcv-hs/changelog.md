# Changelog

All notable changes to this project will be documented in this file.


## 2025-01-15 - 0.1

- Initial release of Haskell bindings for FlatCV
- Bindings for all public FlatCV functions:
  - Color conversion (grayscale, contrast stretching)
  - Filtering (Gaussian blur)
  - Thresholding (Otsu, global threshold)
  - Edge detection (Sobel, Foerstner)
  - Morphological operations (dilation, erosion, opening, closing)
  - Geometric transforms (flip, rotate, transpose, resize)
  - Cropping and trimming
  - Perspective transforms and document extraction
  - Drawing primitives (circles, disks, borders)
  - Histogram generation
  - Watershed segmentation
  - EXIF orientation reading
- High-level Haskell API with `Data.Vector.Storable` integration
- Type-safe wrappers for all C structures (Corners, Matrix3x3, Point2D)
