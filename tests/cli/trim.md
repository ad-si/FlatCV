## Image Trimming

The `trim` operation removes border pixels that have the same color, repeatedly until no more uniform borders can be removed.

Input | Output
------|--------
![](imgs/blob_with_border.png) | ![](imgs/blob_with_border_trim.png)


### Basic Trim Operation

For this test, we'll create a simple image with a uniform border and demonstrate the trim functionality:

```scrut
$ ./flatcv imgs/blob_with_border.png trim imgs/blob_with_border_trim.png
Loaded image: 256x256 with 3 channels
Executing pipeline with 1 operations:
Applying operation: trim
  → Completed in \d+.\d+ ms \(output: \d+x\d+\) (regex)
Final output dimensions: \d+x\d+ (regex)
Successfully saved processed image to 'imgs/blob_with_border_trim.png'
```


### Trim with Threshold

The `trim` operation accepts an optional threshold parameter to handle images with JPEG artifacts or slight vignetting. The threshold is specified as a percentage (e.g., `2%`), which defines the maximum color variation allowed when trimming borders.

This is useful when:
- JPEG compression has introduced minor artifacts around the edges
- The image has slight vignetting (gradual darkening at the borders)
- Scanner or camera artifacts cause minor color variations in the border

```scrut
$ ./flatcv imgs/blob_with_border.png 'trim 2%' imgs/blob_with_border_trim_2pct.png
Loaded image: 256x256 with 3 channels
Executing pipeline with 1 operations:
Applying operation: trim with parameter: 2%
  → Completed in \d+.\d+ ms \(output: \d+x\d+\) (regex)
Final output dimensions: \d+x\d+ (regex)
Successfully saved processed image to 'imgs/blob_with_border_trim_2pct.png'
```

You can also specify the threshold as a plain number (interpreted as percentage):

```scrut
$ ./flatcv imgs/blob_with_border.png 'trim 5' imgs/blob_with_border_trim_5.png
Loaded image: 256x256 with 3 channels
Executing pipeline with 1 operations:
Applying operation: trim with parameter: 5.* (regex)
  → Completed in \d+.\d+ ms \(output: \d+x\d+\) (regex)
Final output dimensions: \d+x\d+ (regex)
Successfully saved processed image to 'imgs/blob_with_border_trim_5.png'
```
