{-# LANGUAGE OverloadedStrings #-}

module Main where

import Data.Word (Word8)
import qualified Data.Vector.Storable as V
import Test.Hspec

import FlatCV

-- | Create a simple test image (4x4 RGBA)
testImage :: V.Vector Word8
testImage = V.fromList
  [ 255, 0, 0, 255,    100, 0, 0, 255,    50, 0, 0, 255,     0, 0, 0, 255
  , 0, 255, 0, 255,    100, 100, 0, 255,  50, 50, 0, 255,    0, 50, 0, 255
  , 0, 0, 255, 255,    0, 100, 100, 255,  0, 50, 50, 255,    0, 0, 50, 255
  , 255, 255, 255, 255, 200, 200, 200, 255, 100, 100, 100, 255, 0, 0, 0, 255
  ]

-- | Create a white image with a black border for trim testing
borderedImage :: V.Vector Word8
borderedImage = V.fromList
  [ 0, 0, 0, 255,      0, 0, 0, 255,      0, 0, 0, 255,      0, 0, 0, 255
  , 0, 0, 0, 255,      255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255
  , 0, 0, 0, 255,      255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255
  , 0, 0, 0, 255,      0, 0, 0, 255,      0, 0, 0, 255,      0, 0, 0, 255
  ]

main :: IO ()
main = hspec $ do
  describe "FlatCV" $ do
    describe "grayscale" $ do
      it "converts an RGBA image to grayscale" $ do
        result <- grayscale 4 4 testImage
        V.length result `shouldBe` 64  -- 4x4x4 channels

      it "produces valid grayscale values" $ do
        result <- grayscale 4 4 testImage
        -- First pixel (255, 0, 0) should become grayish
        -- Gray = 0.299*R + 0.587*G + 0.114*B ≈ 76
        let r = result V.! 0
            g = result V.! 1
            b = result V.! 2
        -- R, G, B should all be equal for grayscale
        r `shouldBe` g
        g `shouldBe` b

    describe "gaussianBlur" $ do
      it "returns an image of the same size" $ do
        result <- gaussianBlur 4 4 1.0 testImage
        V.length result `shouldBe` 64

      it "smooths the image" $ do
        result <- gaussianBlur 4 4 2.0 testImage
        -- The result should be different from the input
        result `shouldNotBe` testImage

    describe "flipX" $ do
      it "returns an image of the same size" $ do
        result <- flipX 4 4 testImage
        V.length result `shouldBe` 64

      it "flipping twice returns the original" $ do
        once <- flipX 4 4 testImage
        twice <- flipX 4 4 once
        twice `shouldBe` testImage

    describe "flipY" $ do
      it "returns an image of the same size" $ do
        result <- flipY 4 4 testImage
        V.length result `shouldBe` 64

      it "flipping twice returns the original" $ do
        once <- flipY 4 4 testImage
        twice <- flipY 4 4 once
        twice `shouldBe` testImage

    describe "rotate180" $ do
      it "returns an image of the same size" $ do
        result <- rotate180 4 4 testImage
        V.length result `shouldBe` 64

      it "rotating twice returns the original" $ do
        once <- rotate180 4 4 testImage
        twice <- rotate180 4 4 once
        twice `shouldBe` testImage

    describe "rotate90CW" $ do
      it "returns an image of the same size for square images" $ do
        result <- rotate90CW 4 4 testImage
        V.length result `shouldBe` 64

      it "rotating four times returns the original" $ do
        r1 <- rotate90CW 4 4 testImage
        r2 <- rotate90CW 4 4 r1
        r3 <- rotate90CW 4 4 r2
        r4 <- rotate90CW 4 4 r3
        r4 `shouldBe` testImage

    describe "crop" $ do
      it "crops a region from the image" $ do
        result <- crop 4 4 4 1 1 2 2 testImage
        V.length result `shouldBe` 16  -- 2x2x4 channels

    describe "otsuThreshold" $ do
      it "returns a binary image" $ do
        result <- otsuThreshold 4 4 False testImage
        V.length result `shouldBe` 64

    describe "bwSmart" $ do
      it "returns an anti-aliased binary image" $ do
        result <- bwSmart 4 4 False testImage
        V.length result `shouldBe` 64

    describe "sobelEdgeDetection" $ do
      it "detects edges in an image" $ do
        result <- sobelEdgeDetection 4 4 4 testImage
        V.length result `shouldBe` 64

    describe "rgbaToGrayscale" $ do
      it "converts to single-channel grayscale" $ do
        result <- rgbaToGrayscale 4 4 testImage
        V.length result `shouldBe` 16  -- 4x4x1 channel

    describe "singleToMultichannel" $ do
      it "converts single-channel to RGBA" $ do
        gray <- rgbaToGrayscale 4 4 testImage
        result <- singleToMultichannel 4 4 gray
        V.length result `shouldBe` 64

    describe "parseHexColor" $ do
      it "parses hex color with hash" $ do
        (r, g, b) <- parseHexColor "#FF0000"
        r `shouldBe` 255
        g `shouldBe` 0
        b `shouldBe` 0

      it "parses hex color without hash" $ do
        (r, g, b) <- parseHexColor "00FF00"
        r `shouldBe` 0
        g `shouldBe` 255
        b `shouldBe` 0

      it "parses blue color" $ do
        (r, g, b) <- parseHexColor "0000FF"
        r `shouldBe` 0
        g `shouldBe` 0
        b `shouldBe` 255

    describe "resize" $ do
      it "scales up an image" $ do
        (outW, outH, result) <- resize 4 4 2.0 2.0 testImage
        outW `shouldBe` 8
        outH `shouldBe` 8
        V.length result `shouldBe` 256  -- 8x8x4

      it "scales down an image" $ do
        (outW, outH, result) <- resize 4 4 0.5 0.5 testImage
        outW `shouldBe` 2
        outH `shouldBe` 2
        V.length result `shouldBe` 16  -- 2x2x4

    describe "trim" $ do
      it "trims border pixels" $ do
        (outW, outH, result) <- trim 4 4 4 borderedImage
        -- The trim function finds the bounding box of non-border pixels
        -- The inner 2x2 white area gets trimmed to remove the black border
        outW `shouldSatisfy` (> 0)
        outH `shouldSatisfy` (> 0)
        V.length result `shouldBe` fromIntegral (outW * outH * 4)

    describe "Corners" $ do
      it "has correct Storable instance" $ do
        let corners = Corners 0 1 2 3 4 5 6 7
        tlX corners `shouldBe` 0
        tlY corners `shouldBe` 1
        trX corners `shouldBe` 2
        trY corners `shouldBe` 3
        brX corners `shouldBe` 4
        brY corners `shouldBe` 5
        blX corners `shouldBe` 6
        blY corners `shouldBe` 7

    describe "Matrix3x3" $ do
      it "has correct Storable instance" $ do
        let mat = Matrix3x3 1 0 0 0 1 0 0 0 1  -- Identity matrix
        m00 mat `shouldBe` 1
        m11 mat `shouldBe` 1
        m22 mat `shouldBe` 1
        m01 mat `shouldBe` 0

    describe "calculatePerspectiveTransform" $ do
      it "calculates identity-like transform for same corners" $ do
        let src = Corners 0 0 100 0 100 100 0 100
            dst = Corners 0 0 100 0 100 100 0 100
        mat <- calculatePerspectiveTransform src dst
        -- Diagonal should be close to 1 for identity-like transform
        abs (m00 mat - 1) `shouldSatisfy` (< 0.001)
        abs (m11 mat - 1) `shouldSatisfy` (< 0.001)

    describe "binaryDilationDisk" $ do
      it "returns an image of the same size" $ do
        result <- binaryDilationDisk 4 4 1 testImage
        V.length result `shouldBe` 64

    describe "binaryErosionDisk" $ do
      it "returns an image of the same size" $ do
        result <- binaryErosionDisk 4 4 1 testImage
        V.length result `shouldBe` 64

    describe "binaryClosingDisk" $ do
      it "returns an image of the same size" $ do
        result <- binaryClosingDisk 4 4 1 testImage
        V.length result `shouldBe` 64

    describe "binaryOpeningDisk" $ do
      it "returns an image of the same size" $ do
        result <- binaryOpeningDisk 4 4 1 testImage
        V.length result `shouldBe` 64

    describe "generateHistogram" $ do
      it "generates a histogram image" $ do
        (outW, outH, result) <- generateHistogram 4 4 4 testImage
        outW `shouldSatisfy` (> 0)
        outH `shouldSatisfy` (> 0)
        V.length result `shouldBe` fromIntegral (outW * outH * 4)

    describe "addBorder" $ do
      it "adds a border around the image" $ do
        (outW, outH, result) <- addBorder 4 4 4 "#FF0000" 2 testImage
        outW `shouldBe` 8  -- 4 + 2*2
        outH `shouldBe` 8
        V.length result `shouldBe` 256  -- 8x8x4

    describe "convertToBinary" $ do
      it "converts to binary with custom colors" $ do
        result <- convertToBinary 4 4 "#FFFFFF" "#000000" testImage
        V.length result `shouldBe` 64
