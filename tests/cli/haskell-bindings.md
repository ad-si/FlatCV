# Haskell Bindings

Haskell bindings for FlatCV are available on
[Hackage](https://hackage.haskell.org/package/flatcv) as the `flatcv` package.


## Installation

Using cabal:

```sh
cabal install flatcv
```

Using stack:

```sh
stack install flatcv
```


## Usage

```hs
import FlatCV
import qualified Data.Vector.Storable as V

main :: IO ()
main = do
  -- Assuming you have RGBA image data as a Vector Word8
  let
    width = 640
    height = 480
    -- imageData :: V.Vector Word8

  -- Convert to grayscale
  grayData <- grayscale width height imageData

  -- Apply Gaussian blur
  blurredData <- gaussianBlur width height 2.0 imageData

  -- Detect edges
  edges <- sobelEdgeDetection width height 4 imageData

  -- Extract a document from a photo
  (outW, outH, docData) <- extractDocumentAuto width height imageData
```

The bindings use `Data.Vector.Storable` for efficient memory handling
with automatic memory management via foreign pointers.


## Available Functions

### Color Conversion

- `grayscale` - Convert RGBA to grayscale (4-channel output)
- `grayscaleStretch` - Grayscale with contrast stretching
- `rgbaToGrayscale` - Convert to single-channel grayscale
- `singleToMultichannel` - Convert grayscale to RGBA


### Filtering

- `gaussianBlur` - Apply Gaussian blur


### Thresholding

- `otsuThreshold` - Otsu's automatic thresholding
- `bwSmart` - Smart black & white conversion
- `applyGlobalThreshold` - Apply fixed threshold


### Edge Detection

- `sobelEdgeDetection` - Sobel edge detection
- `foerstnerCorner` - Foerstner corner detection


### Morphological Operations

- `binaryDilationDisk` - Binary dilation
- `binaryErosionDisk` - Binary erosion
- `binaryClosingDisk` - Binary closing
- `binaryOpeningDisk` - Binary opening


### Geometric Transforms

- `flipX`, `flipY` - Flip horizontally/vertically
- `transpose`, `transverse` - Transpose operations
- `rotate90CW`, `rotate180`, `rotate270CW` - Rotations
- `resize` - Resize with bilinear interpolation


### Cropping & Trimming

- `crop` - Crop rectangular region
- `trim` - Auto-trim borders
- `trimThreshold` - Trim with threshold tolerance


### Perspective Transform

- `calculatePerspectiveTransform` - Calculate transform matrix
- `applyMatrix3x3` - Apply 3x3 transformation
- `detectCorners` - Detect document corners


### Document Extraction

- `extractDocument` - Extract with fixed output size
- `extractDocumentAuto` - Extract with automatic sizing


### Drawing

- `drawCircle` - Draw circle outline
- `drawDisk` - Draw filled circle
- `addBorder` - Add border around image


### Analysis

- `generateHistogram` - Generate histogram visualization


### Segmentation

- `watershedSegmentation` - Watershed segmentation


### Utility

- `getExifOrientation` - Read JPEG EXIF orientation
- `parseHexColor` - Parse hex color string
- `convertToBinary` - Convert to binary with custom colors


## Pointer-Based API

For zero-copy interop with foreign data, a pointer-based API is also available.
These functions work directly with `Ptr Word8` and the caller is responsible
for memory management.

```hs
import FlatCV
import Foreign.Ptr (Ptr)
import Foreign.Marshal.Alloc (free)
import Data.Word (Word8)

processImage :: Ptr Word8 -> IO (Ptr Word8)
processImage inputPtr = do
  -- Output is allocated by C and must be freed by caller
  outputPtr <- grayscalePtr 640 480 inputPtr
  -- ... use outputPtr ...
  free outputPtr
```

Available pointer-based functions include:
`grayscalePtr`, `grayscaleStretchPtr`, `rgbaToGrayscalePtr`,
`singleToMultichannelPtr`, `gaussianBlurPtr`, `otsuThresholdPtr`,
`bwSmartPtr`, `flipXPtr`, `flipYPtr`, `transposePtr`,
`rotate90CWPtr`, `rotate180Ptr`, `rotate270CWPtr`,
`calculatePerspectiveTransformPtr`, `applyMatrix3x3Ptr`, `detectCornersPtr`.


## Types

The library provides several data types for working with images:

- `Image` - An image with dimensions and pixel data
- `Point2D` - A 2D point with x and y coordinates
- `Corners` - Quadrilateral corners (top-left, top-right, bottom-right, bottom-left)
- `Matrix3x3` - A 3x3 transformation matrix


## Links

- [Hackage Documentation](https://hackage.haskell.org/package/flatcv)
- [Source Code](https://github.com/ad-si/FlatCV/tree/main/bindings/flatcv-hs)
