## EXIF Orientation Handling

FlatCV auto-applies the EXIF orientation tag before running the pipeline,
for both JPEG (APP1 segment) and PNG (`eXIf` chunk) inputs.

### JPEG with Orientation 6 (90° CW)

The stored 1200×1800 image is rotated to 1800×1200 before processing.

```scrut
$ ./flatcv tests/Landscape_6.jpg "rotate 0" imgs/exif_jpeg_6.png
Applying EXIF orientation: 6
Loaded image: 1800x1200 with 3 channels
Executing pipeline with 1 operations:
Applying operation: rotate with parameter: 0.0+ (regex)
  → Completed in \d+.\d+ ms \(output: 1800x1200\) (regex)
Final output dimensions: 1800x1200
Successfully saved processed image to 'imgs/exif_jpeg_6.png'
```


### PNG with Orientation 6 (90° CW)

PNG stores EXIF in an `eXIf` chunk; the same orientation is applied.

```scrut
$ ./flatcv tests/Landscape_6.png "rotate 0" imgs/exif_png_6.png
Applying EXIF orientation: 6
Loaded image: 50x100 with 3 channels
Executing pipeline with 1 operations:
Applying operation: rotate with parameter: 0.0+ (regex)
  → Completed in \d+.\d+ ms \(output: 50x100\) (regex)
Final output dimensions: 50x100
Successfully saved processed image to 'imgs/exif_png_6.png'
```


### PNG with Orientation 3 (180°)

A 180° rotation keeps the 100×50 dimensions.

```scrut
$ ./flatcv tests/Landscape_3.png "rotate 0" imgs/exif_png_3.png
Applying EXIF orientation: 3
Loaded image: 100x50 with 3 channels
Executing pipeline with 1 operations:
Applying operation: rotate with parameter: 0.0+ (regex)
  → Completed in \d+.\d+ ms \(output: 100x50\) (regex)
Final output dimensions: 100x50
Successfully saved processed image to 'imgs/exif_png_3.png'
```
