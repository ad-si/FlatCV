# Changelog

## 2026-01-15 - 0.3.0

- Add and improve several image manipulation features
  - Rotate
  - Border
  - Morphological operations: erode, dilate, open, and close
    - Fix binary_erosion_disk function
  - Histogram
  - Add threshold parameter to trim operation
  - Corner detection
    - Improve results by gradually lowering threshold for corner peaks
        and interpolating missing ones
- Add support for images with EXIF rotation
- Add input validation, NULL checks, and integer overflow protection
- Add cross-platform CI testing
- Fix macOS build in makefile
- Add more examples to documentation


## 2025-08-23 - 0.2.0

- Add new image manipulation features
  - Resize
  - Crop
  - Trim
  - Flip x and flip y
  - Draw circles and disks
  - Sobel edge detection
  - Foerstner corner detection
    - Draw corners
  - Watershed segmentation
    - Extract document
- Host documentation site at [flatcv.ad-si.com](https://flatcv.ad-si.com/)
  - Includes an interactive playground page with wasm build of FlatCV
- Add a [benchmark](https://flatcv.ad-si.com/benchmark.html)
    comparing it with GM and ImageMagick
- Rename all functions to start with `fcv_`
- Print warning if binarized image is saved as JPEG
- Use ISC license
- Log run time for each step in image processing pipeline
- Fix blur function
