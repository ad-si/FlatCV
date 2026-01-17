{-# LANGUAGE RecordWildCards #-}

-- |
-- Haskell bindings for the FlatCV image processing library.
--
-- FlatCV is a lightweight, dependency-free image processing library
-- useful for document scanning, image preprocessing, and basic
-- computer vision tasks.
--
-- = Usage Example
--
-- @
-- import FlatCV
-- import qualified Data.Vector.Storable as V
--
-- main :: IO ()
-- main = do
--     -- Load your image data as a Vector of Word8 (RGBA format)
--     let width = 640
--         height = 480
--         imageData = ... -- your RGBA pixel data
--
--     -- Convert to grayscale
--     grayData <- grayscale width height imageData
--
--     -- Apply Gaussian blur
--     blurredData <- gaussianBlur width height 2.0 imageData
-- @
--
module FlatCV
  ( -- * Types
    Image(..)
  , Corners(..)
  , Matrix3x3(..)
  , Point2D(..)

    -- * Pretty printing
  , prettyShowCorners
  , prettyShowMatrix3x3

    -- * Color conversion
  , grayscale
  , grayscaleStretch
  , rgbaToGrayscale
  , singleToMultichannel

    -- * Filtering
  , gaussianBlur

    -- * Thresholding
  , otsuThreshold
  , bwSmart
  , applyGlobalThreshold

    -- * Edge detection
  , sobelEdgeDetection
  , foerstnerCorner

    -- * Morphological operations
  , binaryDilationDisk
  , binaryErosionDisk
  , binaryClosingDisk
  , binaryOpeningDisk

    -- * Geometric transforms
  , flipX
  , flipY
  , transpose
  , transverse
  , rotate90CW
  , rotate180
  , rotate270CW
  , resize

    -- * Cropping and trimming
  , crop
  , trim
  , trimThreshold

    -- * Perspective transform
  , calculatePerspectiveTransform
  , applyMatrix3x3
  , detectCorners

    -- * Document extraction
  , extractDocument
  , extractDocumentAuto

    -- * Drawing
  , drawCircle
  , drawDisk
  , addBorder

    -- * Analysis
  , generateHistogram

    -- * Segmentation
  , watershedSegmentation

    -- * EXIF
  , getExifOrientation

    -- * Utility
  , parseHexColor
  , convertToBinary

    -- * Pointer-based API (zero-copy)
    -- | These functions work directly with 'Ptr Word8' for zero-copy
    -- interop with foreign data. The caller is responsible for memory
    -- management. Output pointers are allocated by the C library and
    -- must be freed with 'Foreign.Marshal.Alloc.free'.

    -- ** Color conversion (pointer-based)
  , grayscalePtr
  , grayscaleStretchPtr
  , rgbaToGrayscalePtr
  , singleToMultichannelPtr

    -- ** Filtering (pointer-based)
  , gaussianBlurPtr

    -- ** Thresholding (pointer-based)
  , otsuThresholdPtr
  , bwSmartPtr

    -- ** Perspective transform (pointer-based)
  , calculatePerspectiveTransformPtr
  , applyMatrix3x3Ptr
  , detectCornersPtr

    -- ** Geometric transforms (pointer-based)
  , flipXPtr
  , flipYPtr
  , transposePtr
  , rotate90CWPtr
  , rotate180Ptr
  , rotate270CWPtr
  ) where

import Control.Monad (when)
import Data.Int (Int32)
import Data.Word (Word8, Word32)
import qualified Data.Vector.Storable as V
import qualified Data.Vector.Storable.Mutable as MV
import Foreign.C.String (withCString)
import Foreign.C.Types (CUChar)
import Foreign.ForeignPtr (newForeignPtr, FinalizerPtr, castForeignPtr)
import Foreign.Marshal.Alloc (alloca, free, mallocBytes)
import Foreign.Marshal.Array (pokeArray)
import Foreign.Ptr (Ptr, castPtr, nullPtr)
import Foreign.Storable (Storable(..), peek, poke, sizeOf, alignment)
import Text.Printf (printf)

import qualified FlatCV.Raw as Raw

-- | An image with dimensions and pixel data
data Image = Image
  { imageWidth    :: {-# UNPACK #-} !Word32
  , imageHeight   :: {-# UNPACK #-} !Word32
  , imageChannels :: {-# UNPACK #-} !Word32
  , imageData     :: !(V.Vector Word8)
  } deriving (Show, Eq)

-- | 2D point
data Point2D = Point2D
  { pointX :: {-# UNPACK #-} !Double
  , pointY :: {-# UNPACK #-} !Double
  } deriving (Show, Eq)

instance Storable Point2D where
  sizeOf _ = 2 * sizeOf (0.0 :: Double)
  alignment _ = alignment (0.0 :: Double)
  peek ptr = do
    x <- peekByteOff ptr 0
    y <- peekByteOff ptr 8
    return $ Point2D x y
  poke ptr Point2D{..} = do
    pokeByteOff ptr 0 pointX
    pokeByteOff ptr 8 pointY

-- | Quadrilateral corners (top-left, top-right, bottom-right, bottom-left)
data Corners = Corners
  { tlX :: {-# UNPACK #-} !Double
  , tlY :: {-# UNPACK #-} !Double
  , trX :: {-# UNPACK #-} !Double
  , trY :: {-# UNPACK #-} !Double
  , brX :: {-# UNPACK #-} !Double
  , brY :: {-# UNPACK #-} !Double
  , blX :: {-# UNPACK #-} !Double
  , blY :: {-# UNPACK #-} !Double
  } deriving (Show, Eq)

instance Storable Corners where
  sizeOf _ = 8 * sizeOf (0.0 :: Double)
  alignment _ = alignment (0.0 :: Double)
  peek ptr = do
    tlX <- peekByteOff ptr 0
    tlY <- peekByteOff ptr 8
    trX <- peekByteOff ptr 16
    trY <- peekByteOff ptr 24
    brX <- peekByteOff ptr 32
    brY <- peekByteOff ptr 40
    blX <- peekByteOff ptr 48
    blY <- peekByteOff ptr 56
    return Corners{..}
  poke ptr Corners{..} = do
    pokeByteOff ptr 0 tlX
    pokeByteOff ptr 8 tlY
    pokeByteOff ptr 16 trX
    pokeByteOff ptr 24 trY
    pokeByteOff ptr 32 brX
    pokeByteOff ptr 40 brY
    pokeByteOff ptr 48 blX
    pokeByteOff ptr 56 blY

-- | Pretty print corners for debugging
prettyShowCorners :: Corners -> String
prettyShowCorners Corners{..} =
  show (tlX, tlY) ++ " " ++ show (trX, trY) ++ "\n" ++
  show (blX, blY) ++ " " ++ show (brX, brY)

-- | 3x3 transformation matrix
data Matrix3x3 = Matrix3x3
  { m00 :: {-# UNPACK #-} !Double
  , m01 :: {-# UNPACK #-} !Double
  , m02 :: {-# UNPACK #-} !Double
  , m10 :: {-# UNPACK #-} !Double
  , m11 :: {-# UNPACK #-} !Double
  , m12 :: {-# UNPACK #-} !Double
  , m20 :: {-# UNPACK #-} !Double
  , m21 :: {-# UNPACK #-} !Double
  , m22 :: {-# UNPACK #-} !Double
  } deriving (Show, Eq)

instance Storable Matrix3x3 where
  sizeOf _ = 9 * sizeOf (0.0 :: Double)
  alignment _ = alignment (0.0 :: Double)
  peek ptr = do
    m00 <- peekByteOff ptr 0
    m01 <- peekByteOff ptr 8
    m02 <- peekByteOff ptr 16
    m10 <- peekByteOff ptr 24
    m11 <- peekByteOff ptr 32
    m12 <- peekByteOff ptr 40
    m20 <- peekByteOff ptr 48
    m21 <- peekByteOff ptr 56
    m22 <- peekByteOff ptr 64
    return Matrix3x3{..}
  poke ptr Matrix3x3{..} = do
    pokeByteOff ptr 0 m00
    pokeByteOff ptr 8 m01
    pokeByteOff ptr 16 m02
    pokeByteOff ptr 24 m10
    pokeByteOff ptr 32 m11
    pokeByteOff ptr 40 m12
    pokeByteOff ptr 48 m20
    pokeByteOff ptr 56 m21
    pokeByteOff ptr 64 m22

-- | Pretty print 3x3 matrix for debugging
prettyShowMatrix3x3 :: Matrix3x3 -> String
prettyShowMatrix3x3 Matrix3x3{..} =
  let fNum n = printf "% .5f" (n :: Double)
  in fNum m00 ++ " " ++ fNum m01 ++ " " ++ fNum m02 ++ "\n" ++
     fNum m10 ++ " " ++ fNum m11 ++ " " ++ fNum m12 ++ "\n" ++
     fNum m20 ++ " " ++ fNum m21 ++ " " ++ fNum m22

-- Foreign finalizer for C-allocated memory
foreign import ccall "stdlib.h &free" finalizerFree :: FinalizerPtr CUChar

-- Helper to create a Vector from a C-allocated buffer
makeVector :: Ptr Word8 -> Int -> IO (V.Vector Word8)
makeVector outPtr len = do
  fp <- newForeignPtr finalizerFree (castPtr outPtr)
  return $ V.unsafeFromForeignPtr0 (castForeignPtr fp) len

-- Helper to run a function that returns a new image buffer
withImageData :: V.Vector Word8 -> (Ptr Word8 -> IO (Ptr Word8)) -> Word32 -> IO (V.Vector Word8)
withImageData input action outputLen = do
  V.unsafeWith input $ \inPtr -> do
    outPtr <- action inPtr
    when (outPtr == nullPtr) $ error "FlatCV: operation failed (null pointer returned)"
    makeVector outPtr (fromIntegral outputLen)

--------------------------------------------------------------------------------
-- Color conversion
--------------------------------------------------------------------------------

-- | Convert RGBA image to grayscale (output is still 4-channel RGBA)
grayscale :: Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
grayscale width height input =
  withImageData input (\p -> Raw.fcv_grayscale width height p) (width * height * 4)

-- | Convert to grayscale with contrast stretching
grayscaleStretch :: Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
grayscaleStretch width height input =
  withImageData input (\p -> Raw.fcv_grayscale_stretch width height p) (width * height * 4)

-- | Convert RGBA to single-channel grayscale
rgbaToGrayscale :: Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
rgbaToGrayscale width height input =
  withImageData input (\p -> Raw.fcv_rgba_to_grayscale width height p) (width * height)

-- | Convert single-channel grayscale to RGBA
singleToMultichannel :: Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
singleToMultichannel width height input =
  withImageData input (\p -> Raw.fcv_single_to_multichannel width height p) (width * height * 4)

--------------------------------------------------------------------------------
-- Filtering
--------------------------------------------------------------------------------

-- | Apply Gaussian blur with given radius
gaussianBlur :: Word32 -> Word32 -> Double -> V.Vector Word8 -> IO (V.Vector Word8)
gaussianBlur width height radius input =
  withImageData input (\p -> Raw.fcv_apply_gaussian_blur width height radius p) (width * height * 4)

--------------------------------------------------------------------------------
-- Thresholding
--------------------------------------------------------------------------------

-- | Apply Otsu threshold to convert image to binary
-- If useDoubleThreshold is True, uses double thresholding for better results
otsuThreshold :: Word32 -> Word32 -> Bool -> V.Vector Word8 -> IO (V.Vector Word8)
otsuThreshold width height useDouble input =
  withImageData input (\p -> Raw.fcv_otsu_threshold_rgba width height useDouble p) (width * height * 4)

-- | Smart black and white conversion with anti-aliasing
bwSmart :: Word32 -> Word32 -> Bool -> V.Vector Word8 -> IO (V.Vector Word8)
bwSmart width height useDouble input =
  withImageData input (\p -> Raw.fcv_bw_smart width height useDouble p) (width * height * 4)

-- | Apply global threshold in-place
applyGlobalThreshold :: Word32 -> Word8 -> V.Vector Word8 -> IO (V.Vector Word8)
applyGlobalThreshold imgLen threshold input = do
  output <- V.thaw input
  MV.unsafeWith output $ \ptr ->
    Raw.fcv_apply_global_threshold imgLen ptr threshold
  V.unsafeFreeze output

--------------------------------------------------------------------------------
-- Edge detection
--------------------------------------------------------------------------------

-- | Apply Sobel edge detection
sobelEdgeDetection :: Word32 -> Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
sobelEdgeDetection width height channels input =
  withImageData input (\p -> Raw.fcv_sobel_edge_detection width height channels p) (width * height * channels)

-- | Foerstner corner detection with given sigma
foerstnerCorner :: Word32 -> Word32 -> Double -> V.Vector Word8 -> IO (V.Vector Word8)
foerstnerCorner width height sigma input =
  withImageData input (\p -> Raw.fcv_foerstner_corner width height p sigma) (width * height * 4)

--------------------------------------------------------------------------------
-- Morphological operations
--------------------------------------------------------------------------------

-- | Binary dilation with disk structuring element
binaryDilationDisk :: Int32 -> Int32 -> Int32 -> V.Vector Word8 -> IO (V.Vector Word8)
binaryDilationDisk width height radius input =
  withImageData input (\p -> Raw.fcv_binary_dilation_disk p width height radius)
    (fromIntegral $ width * height * 4)

-- | Binary erosion with disk structuring element
binaryErosionDisk :: Int32 -> Int32 -> Int32 -> V.Vector Word8 -> IO (V.Vector Word8)
binaryErosionDisk width height radius input =
  withImageData input (\p -> Raw.fcv_binary_erosion_disk p width height radius)
    (fromIntegral $ width * height * 4)

-- | Binary closing (dilation followed by erosion)
binaryClosingDisk :: Int32 -> Int32 -> Int32 -> V.Vector Word8 -> IO (V.Vector Word8)
binaryClosingDisk width height radius input =
  withImageData input (\p -> Raw.fcv_binary_closing_disk p width height radius)
    (fromIntegral $ width * height * 4)

-- | Binary opening (erosion followed by dilation)
binaryOpeningDisk :: Int32 -> Int32 -> Int32 -> V.Vector Word8 -> IO (V.Vector Word8)
binaryOpeningDisk width height radius input =
  withImageData input (\p -> Raw.fcv_binary_opening_disk p width height radius)
    (fromIntegral $ width * height * 4)

--------------------------------------------------------------------------------
-- Geometric transforms
--------------------------------------------------------------------------------

-- | Flip image horizontally
flipX :: Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
flipX width height input =
  withImageData input (\p -> Raw.fcv_flip_x width height p) (width * height * 4)

-- | Flip image vertically
flipY :: Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
flipY width height input =
  withImageData input (\p -> Raw.fcv_flip_y width height p) (width * height * 4)

-- | Transpose image (swap rows and columns)
transpose :: Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
transpose width height input =
  withImageData input (\p -> Raw.fcv_transpose width height p) (width * height * 4)

-- | Transverse (transpose + flip both axes)
transverse :: Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
transverse width height input =
  withImageData input (\p -> Raw.fcv_transverse width height p) (width * height * 4)

-- | Rotate image 90 degrees clockwise
rotate90CW :: Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
rotate90CW width height input =
  withImageData input (\p -> Raw.fcv_rotate_90_cw width height p) (width * height * 4)

-- | Rotate image 180 degrees
rotate180 :: Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
rotate180 width height input =
  withImageData input (\p -> Raw.fcv_rotate_180 width height p) (width * height * 4)

-- | Rotate image 270 degrees clockwise (90 counter-clockwise)
rotate270CW :: Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
rotate270CW width height input =
  withImageData input (\p -> Raw.fcv_rotate_270_cw width height p) (width * height * 4)

-- | Resize image with bilinear interpolation
-- Returns (new width, new height, output data)
resize :: Word32 -> Word32 -> Double -> Double -> V.Vector Word8 -> IO (Word32, Word32, V.Vector Word8)
resize width height scaleX scaleY input = do
  alloca $ \outWPtr ->
    alloca $ \outHPtr -> do
      V.unsafeWith input $ \inPtr -> do
        outPtr <- Raw.fcv_resize width height scaleX scaleY outWPtr outHPtr inPtr
        when (outPtr == nullPtr) $ error "FlatCV: resize failed"
        outW <- peek outWPtr
        outH <- peek outHPtr
        outData <- makeVector outPtr (fromIntegral $ outW * outH * 4)
        return (outW, outH, outData)

--------------------------------------------------------------------------------
-- Cropping and trimming
--------------------------------------------------------------------------------

-- | Crop a rectangular region from the image
crop :: Word32 -> Word32 -> Word32 -> Word32 -> Word32 -> Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
crop width height channels x y newWidth newHeight input =
  withImageData input (\p -> Raw.fcv_crop width height channels p x y newWidth newHeight)
    (newWidth * newHeight * channels)

-- | Trim border pixels with same color
-- Returns (new width, new height, output data)
trim :: Int32 -> Int32 -> Word32 -> V.Vector Word8 -> IO (Int32, Int32, V.Vector Word8)
trim width height channels input = do
  alloca $ \wPtr ->
    alloca $ \hPtr -> do
      poke wPtr width
      poke hPtr height
      V.unsafeWith input $ \inPtr -> do
        outPtr <- Raw.fcv_trim wPtr hPtr channels inPtr
        when (outPtr == nullPtr) $ error "FlatCV: trim failed"
        outW <- peek wPtr
        outH <- peek hPtr
        outData <- makeVector outPtr (fromIntegral $ outW * outH * fromIntegral channels)
        return (outW, outH, outData)

-- | Trim border pixels within threshold percentage
-- Returns (new width, new height, output data)
trimThreshold :: Int32 -> Int32 -> Word32 -> Double -> V.Vector Word8 -> IO (Int32, Int32, V.Vector Word8)
trimThreshold width height channels thresholdPercent input = do
  alloca $ \wPtr ->
    alloca $ \hPtr -> do
      poke wPtr width
      poke hPtr height
      V.unsafeWith input $ \inPtr -> do
        outPtr <- Raw.fcv_trim_threshold wPtr hPtr channels inPtr thresholdPercent
        when (outPtr == nullPtr) $ error "FlatCV: trim_threshold failed"
        outW <- peek wPtr
        outH <- peek hPtr
        outData <- makeVector outPtr (fromIntegral $ outW * outH * fromIntegral channels)
        return (outW, outH, outData)

--------------------------------------------------------------------------------
-- Perspective transform
--------------------------------------------------------------------------------

-- | Calculate perspective transformation matrix from source to destination corners
calculatePerspectiveTransform :: Corners -> Corners -> IO Matrix3x3
calculatePerspectiveTransform src dst = do
  alloca $ \srcPtr ->
    alloca $ \dstPtr -> do
      poke srcPtr src
      poke dstPtr dst
      matPtr <- Raw.fcv_calculate_perspective_transform (castPtr srcPtr) (castPtr dstPtr)
      when (matPtr == nullPtr) $ error "FlatCV: perspective transform calculation failed"
      mat <- peek (castPtr matPtr)
      free matPtr
      return mat

-- | Apply 3x3 transformation matrix to image
applyMatrix3x3 :: Int32 -> Int32 -> Int32 -> Int32 -> Matrix3x3 -> V.Vector Word8 -> IO (V.Vector Word8)
applyMatrix3x3 inWidth inHeight outWidth outHeight matrix input = do
  alloca $ \matPtr -> do
    poke matPtr matrix
    withImageData input (\p -> Raw.fcv_apply_matrix_3x3 inWidth inHeight p outWidth outHeight (castPtr matPtr))
      (fromIntegral $ outWidth * outHeight * 4)

-- | Detect document corners in an image
detectCorners :: Int32 -> Int32 -> V.Vector Word8 -> IO Corners
detectCorners width height input = do
  V.unsafeWith input $ \inPtr -> do
    cornersPtr <- Raw.fcv_detect_corners_ptr inPtr width height
    when (cornersPtr == nullPtr) $ error "FlatCV: corner detection failed"
    corners <- peek (castPtr cornersPtr)
    free cornersPtr
    return corners

--------------------------------------------------------------------------------
-- Document extraction
--------------------------------------------------------------------------------

-- | Extract document with specified output dimensions
extractDocument :: Word32 -> Word32 -> Word32 -> Word32 -> V.Vector Word8 -> IO (V.Vector Word8)
extractDocument width height outWidth outHeight input =
  withImageData input (\p -> Raw.fcv_extract_document width height p outWidth outHeight)
    (outWidth * outHeight * 4)

-- | Extract document with automatic size detection
-- Returns (output width, output height, output data)
extractDocumentAuto :: Word32 -> Word32 -> V.Vector Word8 -> IO (Word32, Word32, V.Vector Word8)
extractDocumentAuto width height input = do
  alloca $ \outWPtr ->
    alloca $ \outHPtr -> do
      V.unsafeWith input $ \inPtr -> do
        outPtr <- Raw.fcv_extract_document_auto width height inPtr outWPtr outHPtr
        when (outPtr == nullPtr) $ error "FlatCV: document extraction failed"
        outW <- peek outWPtr
        outH <- peek outHPtr
        outData <- makeVector outPtr (fromIntegral $ outW * outH * 4)
        return (outW, outH, outData)

--------------------------------------------------------------------------------
-- Drawing
--------------------------------------------------------------------------------

-- | Draw a circle outline on an image (modifies input in place)
drawCircle :: Word32 -> Word32 -> Word32 -> String -> Double -> Double -> Double -> V.Vector Word8 -> IO (V.Vector Word8)
drawCircle width height channels hexColor radius centerX centerY input = do
  output <- V.thaw input
  MV.unsafeWith output $ \ptr ->
    withCString hexColor $ \colorPtr ->
      Raw.fcv_draw_circle width height channels colorPtr radius centerX centerY ptr
  V.unsafeFreeze output

-- | Draw a filled disk on an image (modifies input in place)
drawDisk :: Word32 -> Word32 -> Word32 -> String -> Double -> Double -> Double -> V.Vector Word8 -> IO (V.Vector Word8)
drawDisk width height channels hexColor radius centerX centerY input = do
  output <- V.thaw input
  MV.unsafeWith output $ \ptr ->
    withCString hexColor $ \colorPtr ->
      Raw.fcv_draw_disk width height channels colorPtr radius centerX centerY ptr
  V.unsafeFreeze output

-- | Add a border around an image
-- Returns (output width, output height, output data)
addBorder :: Word32 -> Word32 -> Word32 -> String -> Word32 -> V.Vector Word8 -> IO (Word32, Word32, V.Vector Word8)
addBorder width height channels hexColor borderWidth input = do
  alloca $ \outWPtr ->
    alloca $ \outHPtr -> do
      V.unsafeWith input $ \inPtr ->
        withCString hexColor $ \colorPtr -> do
          outPtr <- Raw.fcv_add_border width height channels colorPtr borderWidth inPtr outWPtr outHPtr
          when (outPtr == nullPtr) $ error "FlatCV: add_border failed"
          outW <- peek outWPtr
          outH <- peek outHPtr
          outData <- makeVector outPtr (fromIntegral $ outW * outH * channels)
          return (outW, outH, outData)

--------------------------------------------------------------------------------
-- Analysis
--------------------------------------------------------------------------------

-- | Generate histogram visualization
-- Returns (histogram width, histogram height, RGBA output data)
generateHistogram :: Word32 -> Word32 -> Word32 -> V.Vector Word8 -> IO (Word32, Word32, V.Vector Word8)
generateHistogram width height channels input = do
  alloca $ \outWPtr ->
    alloca $ \outHPtr -> do
      V.unsafeWith input $ \inPtr -> do
        outPtr <- Raw.fcv_generate_histogram width height channels inPtr outWPtr outHPtr
        when (outPtr == nullPtr) $ error "FlatCV: histogram generation failed"
        outW <- peek outWPtr
        outH <- peek outHPtr
        outData <- makeVector outPtr (fromIntegral $ outW * outH * 4)
        return (outW, outH, outData)

--------------------------------------------------------------------------------
-- Segmentation
--------------------------------------------------------------------------------

-- | Watershed segmentation with markers
watershedSegmentation :: Word32 -> Word32 -> [Point2D] -> Bool -> V.Vector Word8 -> IO (V.Vector Word8)
watershedSegmentation width height markers createBoundaries input = do
  let numMarkers = fromIntegral $ length markers
      markerSize = numMarkers * sizeOf (undefined :: Point2D)
  markersPtr <- mallocBytes markerSize
  pokeArray markersPtr markers
  result <- withImageData input
    (\p -> Raw.fcv_watershed_segmentation width height p (castPtr markersPtr) (fromIntegral numMarkers) createBoundaries)
    (width * height * 4)
  free markersPtr
  return result

--------------------------------------------------------------------------------
-- EXIF
--------------------------------------------------------------------------------

-- | Get EXIF orientation from a JPEG file
-- Returns orientation value 1-8, or 1 if not found
getExifOrientation :: FilePath -> IO Int32
getExifOrientation path =
  withCString path Raw.fcv_get_exif_orientation

--------------------------------------------------------------------------------
-- Utility
--------------------------------------------------------------------------------

-- | Parse hex color string (e.g., "#FF0000" or "FF0000") to RGB
parseHexColor :: String -> IO (Word8, Word8, Word8)
parseHexColor hexColor =
  alloca $ \rPtr ->
    alloca $ \gPtr ->
      alloca $ \bPtr ->
        withCString hexColor $ \colorPtr -> do
          Raw.fcv_parse_hex_color colorPtr rPtr gPtr bPtr
          r <- peek rPtr
          g <- peek gPtr
          b <- peek bPtr
          return (r, g, b)

-- | Convert image to binary with custom foreground and background colors
convertToBinary :: Int32 -> Int32 -> String -> String -> V.Vector Word8 -> IO (V.Vector Word8)
convertToBinary width height fgHex bgHex input =
  withCString fgHex $ \fgPtr ->
    withCString bgHex $ \bgPtr ->
      withImageData input (\p -> Raw.fcv_convert_to_binary p width height fgPtr bgPtr)
        (fromIntegral $ width * height * 4)

--------------------------------------------------------------------------------
-- Pointer-based API (zero-copy)
--------------------------------------------------------------------------------

-- | Convert RGBA image to grayscale (pointer-based).
-- Output is allocated by C and must be freed by caller.
grayscalePtr :: Word32 -> Word32 -> Ptr Word8 -> IO (Ptr Word8)
grayscalePtr = Raw.fcv_grayscale

-- | Convert to grayscale with contrast stretching (pointer-based).
-- Output is allocated by C and must be freed by caller.
grayscaleStretchPtr :: Word32 -> Word32 -> Ptr Word8 -> IO (Ptr Word8)
grayscaleStretchPtr = Raw.fcv_grayscale_stretch

-- | Convert RGBA to single-channel grayscale (pointer-based).
-- Output is allocated by C and must be freed by caller.
rgbaToGrayscalePtr :: Word32 -> Word32 -> Ptr Word8 -> IO (Ptr Word8)
rgbaToGrayscalePtr = Raw.fcv_rgba_to_grayscale

-- | Convert single-channel grayscale to RGBA (pointer-based).
-- Output is allocated by C and must be freed by caller.
singleToMultichannelPtr :: Word32 -> Word32 -> Ptr Word8 -> IO (Ptr Word8)
singleToMultichannelPtr = Raw.fcv_single_to_multichannel

-- | Apply Gaussian blur (pointer-based).
-- Output is allocated by C and must be freed by caller.
gaussianBlurPtr :: Word32 -> Word32 -> Double -> Ptr Word8 -> IO (Ptr Word8)
gaussianBlurPtr = Raw.fcv_apply_gaussian_blur

-- | Apply Otsu threshold (pointer-based).
-- Output is allocated by C and must be freed by caller.
otsuThresholdPtr :: Word32 -> Word32 -> Bool -> Ptr Word8 -> IO (Ptr Word8)
otsuThresholdPtr = Raw.fcv_otsu_threshold_rgba

-- | Smart black and white conversion (pointer-based).
-- Output is allocated by C and must be freed by caller.
bwSmartPtr :: Word32 -> Word32 -> Bool -> Ptr Word8 -> IO (Ptr Word8)
bwSmartPtr = Raw.fcv_bw_smart

-- | Calculate perspective transform matrix (pointer-based).
-- Takes pointers to Corners structs, returns pointer to Matrix3x3.
-- Output matrix is allocated by C and must be freed by caller.
calculatePerspectiveTransformPtr :: Ptr Corners -> Ptr Corners -> IO (Ptr Matrix3x3)
calculatePerspectiveTransformPtr srcPtr dstPtr = do
  matPtr <- Raw.fcv_calculate_perspective_transform (castPtr srcPtr) (castPtr dstPtr)
  return (castPtr matPtr)

-- | Apply 3x3 transformation matrix (pointer-based).
-- Output is allocated by C and must be freed by caller.
applyMatrix3x3Ptr
  :: Int32         -- ^ Input width
  -> Int32         -- ^ Input height
  -> Ptr Word8     -- ^ Input data
  -> Int32         -- ^ Output width
  -> Int32         -- ^ Output height
  -> Ptr Matrix3x3 -- ^ Transformation matrix
  -> IO (Ptr Word8)
applyMatrix3x3Ptr inW inH inPtr outW outH matPtr =
  Raw.fcv_apply_matrix_3x3 inW inH inPtr outW outH (castPtr matPtr)

-- | Detect document corners (pointer-based).
-- Output is allocated by C and must be freed by caller.
detectCornersPtr :: Ptr Word8 -> Int32 -> Int32 -> IO (Ptr Corners)
detectCornersPtr inPtr w h = do
  cornersPtr <- Raw.fcv_detect_corners_ptr inPtr w h
  return (castPtr cornersPtr)

-- | Flip image horizontally (pointer-based).
-- Output is allocated by C and must be freed by caller.
flipXPtr :: Word32 -> Word32 -> Ptr Word8 -> IO (Ptr Word8)
flipXPtr = Raw.fcv_flip_x

-- | Flip image vertically (pointer-based).
-- Output is allocated by C and must be freed by caller.
flipYPtr :: Word32 -> Word32 -> Ptr Word8 -> IO (Ptr Word8)
flipYPtr = Raw.fcv_flip_y

-- | Transpose image (pointer-based).
-- Output is allocated by C and must be freed by caller.
transposePtr :: Word32 -> Word32 -> Ptr Word8 -> IO (Ptr Word8)
transposePtr = Raw.fcv_transpose

-- | Rotate 90 degrees clockwise (pointer-based).
-- Output is allocated by C and must be freed by caller.
rotate90CWPtr :: Word32 -> Word32 -> Ptr Word8 -> IO (Ptr Word8)
rotate90CWPtr = Raw.fcv_rotate_90_cw

-- | Rotate 180 degrees (pointer-based).
-- Output is allocated by C and must be freed by caller.
rotate180Ptr :: Word32 -> Word32 -> Ptr Word8 -> IO (Ptr Word8)
rotate180Ptr = Raw.fcv_rotate_180

-- | Rotate 270 degrees clockwise (pointer-based).
-- Output is allocated by C and must be freed by caller.
rotate270CWPtr :: Word32 -> Word32 -> Ptr Word8 -> IO (Ptr Word8)
rotate270CWPtr = Raw.fcv_rotate_270_cw
