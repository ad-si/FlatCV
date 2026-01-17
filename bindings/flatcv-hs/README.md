# FlatCV Haskell Bindings

Haskell bindings for the [FlatCV](https://github.com/ad-si/FlatCV)
image processing library.


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
