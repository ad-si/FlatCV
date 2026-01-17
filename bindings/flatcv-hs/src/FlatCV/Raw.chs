{-# LANGUAGE ForeignFunctionInterface #-}

module FlatCV.Raw where

import Data.Int (Int32)
import Data.Word (Word8, Word32)
import Foreign.C.String (CString)
import Foreign.C.Types (CInt, CUChar, CUInt)
import Foreign.Ptr (Ptr, castPtr)

#include "flatcv.h"

-- | Point2D structure
data Point2D = Point2D
  { point2D_x :: {-# UNPACK #-} !Double
  , point2D_y :: {-# UNPACK #-} !Double
  } deriving (Show, Eq)

{#pointer *Point2D as Point2DPtr -> Point2D#}

-- | Corners structure for quadrilateral coordinates
data Corners = Corners
  { corners_tl_x :: {-# UNPACK #-} !Double
  , corners_tl_y :: {-# UNPACK #-} !Double
  , corners_tr_x :: {-# UNPACK #-} !Double
  , corners_tr_y :: {-# UNPACK #-} !Double
  , corners_br_x :: {-# UNPACK #-} !Double
  , corners_br_y :: {-# UNPACK #-} !Double
  , corners_bl_x :: {-# UNPACK #-} !Double
  , corners_bl_y :: {-# UNPACK #-} !Double
  } deriving (Show, Eq)

{#pointer *Corners as CornersPtr -> Corners#}

-- | 3x3 transformation matrix
data Matrix3x3 = Matrix3x3
  { matrix_m00 :: {-# UNPACK #-} !Double
  , matrix_m01 :: {-# UNPACK #-} !Double
  , matrix_m02 :: {-# UNPACK #-} !Double
  , matrix_m10 :: {-# UNPACK #-} !Double
  , matrix_m11 :: {-# UNPACK #-} !Double
  , matrix_m12 :: {-# UNPACK #-} !Double
  , matrix_m20 :: {-# UNPACK #-} !Double
  , matrix_m21 :: {-# UNPACK #-} !Double
  , matrix_m22 :: {-# UNPACK #-} !Double
  } deriving (Show, Eq)

{#pointer *Matrix3x3 as Matrix3x3Ptr -> Matrix3x3#}

-- | Corner peaks result structure
data CornerPeaks = CornerPeaks
  { cornerPeaks_points :: Ptr Point2D
  , cornerPeaks_count  :: {-# UNPACK #-} !Word32
  } deriving (Show, Eq)

{#pointer *CornerPeaks as CornerPeaksPtr -> CornerPeaks#}

-- Marshalling helpers
toWord8Ptr :: Ptr CUChar -> Ptr Word8
toWord8Ptr = castPtr

fromWord8Ptr :: Ptr Word8 -> Ptr CUChar
fromWord8Ptr = castPtr

toInt32Ptr :: Ptr CInt -> Ptr Int32
toInt32Ptr = castPtr

fromInt32Ptr :: Ptr Int32 -> Ptr CInt
fromInt32Ptr = castPtr

toWord32Ptr :: Ptr CUInt -> Ptr Word32
toWord32Ptr = castPtr

fromWord32Ptr :: Ptr Word32 -> Ptr CUInt
fromWord32Ptr = castPtr

--------------------------------------------------------------------------------
-- Conversion functions
--------------------------------------------------------------------------------

-- | Apply Gaussian blur to an RGBA image
{#fun fcv_apply_gaussian_blur
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , `Double'                      -- ^ radius
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Convert RGBA image to grayscale (still 4-channel output)
{#fun fcv_grayscale
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Convert to grayscale with contrast stretching
{#fun fcv_grayscale_stretch
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Apply global threshold to image data (in-place)
{#fun fcv_apply_global_threshold
  { `Word32'                      -- ^ image length (width * height * channels)
  , fromWord8Ptr `Ptr Word8'      -- ^ data (modified in place)
  , `Word8'                       -- ^ threshold value
  } -> `()'
#}

-- | Apply Otsu threshold to RGBA image
{#fun fcv_otsu_threshold_rgba
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , `Bool'                        -- ^ use double threshold
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Smart black and white conversion
{#fun fcv_bw_smart
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , `Bool'                        -- ^ use double threshold
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Resize image with bilinear interpolation
{#fun fcv_resize
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , `Double'                      -- ^ scale_x
  , `Double'                      -- ^ scale_y
  , fromWord32Ptr `Ptr Word32'    -- ^ out_width pointer
  , fromWord32Ptr `Ptr Word32'    -- ^ out_height pointer
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Binary morphology functions
--------------------------------------------------------------------------------

-- | Binary dilation with disk structuring element
{#fun fcv_binary_dilation_disk
  { fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Int32'                       -- ^ width
  , `Int32'                       -- ^ height
  , `Int32'                       -- ^ radius
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Binary erosion with disk structuring element
{#fun fcv_binary_erosion_disk
  { fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Int32'                       -- ^ width
  , `Int32'                       -- ^ height
  , `Int32'                       -- ^ radius
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Binary closing (dilation then erosion) with disk
{#fun fcv_binary_closing_disk
  { fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Int32'                       -- ^ width
  , `Int32'                       -- ^ height
  , `Int32'                       -- ^ radius
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Binary opening (erosion then dilation) with disk
{#fun fcv_binary_opening_disk
  { fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Int32'                       -- ^ width
  , `Int32'                       -- ^ height
  , `Int32'                       -- ^ radius
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Convert to binary
--------------------------------------------------------------------------------

-- | Convert image to binary with custom colors
{#fun fcv_convert_to_binary
  { fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Int32'                       -- ^ width
  , `Int32'                       -- ^ height
  , id `CString'                  -- ^ foreground hex color
  , id `CString'                  -- ^ background hex color
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Corner detection
--------------------------------------------------------------------------------

-- | Detect corner peaks in an image
{#fun fcv_corner_peaks
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Word32'                      -- ^ min_distance
  , `Double'                      -- ^ accuracy_thresh
  , `Double'                      -- ^ roundness_thresh
  } -> `CornerPeaksPtr'
#}

--------------------------------------------------------------------------------
-- Crop
--------------------------------------------------------------------------------

-- | Crop a region from an image
{#fun fcv_crop
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , `Word32'                      -- ^ channels
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Word32'                      -- ^ x offset
  , `Word32'                      -- ^ y offset
  , `Word32'                      -- ^ new_width
  , `Word32'                      -- ^ new_height
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Drawing functions
--------------------------------------------------------------------------------

-- | Draw a circle outline on an image (in-place)
{#fun fcv_draw_circle
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , `Word32'                      -- ^ channels
  , id `CString'                  -- ^ hex color
  , `Double'                      -- ^ radius
  , `Double'                      -- ^ center_x
  , `Double'                      -- ^ center_y
  , fromWord8Ptr `Ptr Word8'      -- ^ data (modified in place)
  } -> `()'
#}

-- | Draw a filled disk on an image (in-place)
{#fun fcv_draw_disk
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , `Word32'                      -- ^ channels
  , id `CString'                  -- ^ hex color
  , `Double'                      -- ^ radius
  , `Double'                      -- ^ center_x
  , `Double'                      -- ^ center_y
  , fromWord8Ptr `Ptr Word8'      -- ^ data (modified in place)
  } -> `()'
#}

-- | Add a border around an image
{#fun fcv_add_border
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , `Word32'                      -- ^ channels
  , id `CString'                  -- ^ hex color
  , `Word32'                      -- ^ border_width
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  , fromWord32Ptr `Ptr Word32'    -- ^ output_width pointer
  , fromWord32Ptr `Ptr Word32'    -- ^ output_height pointer
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- EXIF
--------------------------------------------------------------------------------

-- | Get EXIF orientation from a JPEG file
{#fun fcv_get_exif_orientation
  { id `CString'                  -- ^ filename
  } -> `Int32'
#}

--------------------------------------------------------------------------------
-- Document extraction
--------------------------------------------------------------------------------

-- | Extract document with fixed output size
{#fun fcv_extract_document
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Word32'                      -- ^ output_width
  , `Word32'                      -- ^ output_height
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Extract document with automatic size detection
{#fun fcv_extract_document_auto
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  , fromWord32Ptr `Ptr Word32'    -- ^ output_width pointer
  , fromWord32Ptr `Ptr Word32'    -- ^ output_height pointer
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Flip and transpose
--------------------------------------------------------------------------------

-- | Flip image horizontally
{#fun fcv_flip_x
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Flip image vertically
{#fun fcv_flip_y
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Transpose image (swap x and y axes)
{#fun fcv_transpose
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Transverse (transpose + flip both axes)
{#fun fcv_transverse
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Rotation
--------------------------------------------------------------------------------

-- | Rotate image 90 degrees clockwise
{#fun fcv_rotate_90_cw
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Rotate image 180 degrees
{#fun fcv_rotate_180
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Rotate image 270 degrees clockwise (90 counter-clockwise)
{#fun fcv_rotate_270_cw
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Foerstner corner detection
--------------------------------------------------------------------------------

-- | Foerstner corner detection
{#fun fcv_foerstner_corner
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Double'                      -- ^ sigma
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Histogram
--------------------------------------------------------------------------------

-- | Generate histogram visualization
{#fun fcv_generate_histogram
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , `Word32'                      -- ^ channels
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  , fromWord32Ptr `Ptr Word32'    -- ^ out_width pointer
  , fromWord32Ptr `Ptr Word32'    -- ^ out_height pointer
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Hex color parsing
--------------------------------------------------------------------------------

-- | Parse hex color string to RGB components
{#fun fcv_parse_hex_color
  { id `CString'                  -- ^ hex color string
  , fromWord8Ptr `Ptr Word8'      -- ^ r output
  , fromWord8Ptr `Ptr Word8'      -- ^ g output
  , fromWord8Ptr `Ptr Word8'      -- ^ b output
  } -> `()'
#}

--------------------------------------------------------------------------------
-- Perspective transform
--------------------------------------------------------------------------------

-- | Calculate perspective transform matrix
{#fun fcv_calculate_perspective_transform
  { `CornersPtr'                  -- ^ source corners
  , `CornersPtr'                  -- ^ destination corners
  } -> `Matrix3x3Ptr'
#}

-- | Apply 3x3 transformation matrix to image
{#fun fcv_apply_matrix_3x3
  { `Int32'                       -- ^ input width
  , `Int32'                       -- ^ input height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Int32'                       -- ^ output width
  , `Int32'                       -- ^ output height
  , `Matrix3x3Ptr'                -- ^ transformation matrix
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Detect document corners in an image
{#fun fcv_detect_corners_ptr
  { fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Int32'                       -- ^ width
  , `Int32'                       -- ^ height
  } -> `CornersPtr'
#}

--------------------------------------------------------------------------------
-- RGBA to grayscale
--------------------------------------------------------------------------------

-- | Convert RGBA to single-channel grayscale
{#fun fcv_rgba_to_grayscale
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Single to multichannel
--------------------------------------------------------------------------------

-- | Convert single-channel to RGBA
{#fun fcv_single_to_multichannel
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Sobel edge detection
--------------------------------------------------------------------------------

-- | Apply Sobel edge detection
{#fun fcv_sobel_edge_detection
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , `Word32'                      -- ^ channels
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Trim
--------------------------------------------------------------------------------

-- | Trim border pixels with same color
{#fun fcv_trim
  { fromInt32Ptr `Ptr Int32'      -- ^ width pointer (modified)
  , fromInt32Ptr `Ptr Int32'      -- ^ height pointer (modified)
  , `Word32'                      -- ^ channels
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  } -> `Ptr Word8' toWord8Ptr
#}

-- | Trim border pixels within threshold
{#fun fcv_trim_threshold
  { fromInt32Ptr `Ptr Int32'      -- ^ width pointer (modified)
  , fromInt32Ptr `Ptr Int32'      -- ^ height pointer (modified)
  , `Word32'                      -- ^ channels
  , fromWord8Ptr `Ptr Word8'      -- ^ input data
  , `Double'                      -- ^ threshold_percent
  } -> `Ptr Word8' toWord8Ptr
#}

--------------------------------------------------------------------------------
-- Watershed segmentation
--------------------------------------------------------------------------------

-- | Watershed segmentation
{#fun fcv_watershed_segmentation
  { `Word32'                      -- ^ width
  , `Word32'                      -- ^ height
  , fromWord8Ptr `Ptr Word8'      -- ^ grayscale data
  , `Point2DPtr'                  -- ^ markers array
  , `Word32'                      -- ^ num_markers
  , `Bool'                        -- ^ create_boundaries
  } -> `Ptr Word8' toWord8Ptr
#}
