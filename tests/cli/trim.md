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
