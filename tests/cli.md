# CLI Tests

## Parrot Image

### Grayscale Conversion

```scrut
$ ./flatcv imgs/parrot.jpeg grayscale imgs/parrot_grayscale.jpeg
Loaded image: 512x512 with 3 channels
Executing pipeline with 1 operations:
Applying operation: grayscale
  → Completed in \d+.\d+ ms \(output: 512x512\) (regex)
Final output dimensions: 512x512
Successfully saved processed image to 'imgs/parrot_grayscale.jpeg'
```

```scrut
$ git diff --quiet imgs/parrot_grayscale.jpeg
```


### Gaussian Blur

```scrut
$ ./flatcv imgs/parrot.jpeg blur 9 imgs/parrot_blur.jpeg
Loaded image: 512x512 with 3 channels
Executing pipeline with 1 operations:
Applying operation: blur with parameter: 9.00
  → Completed in \d+.\d+ ms \(output: 512x512\) (regex)
Final output dimensions: 512x512
Successfully saved processed image to 'imgs/parrot_blur.jpeg'
```

```scrut
$ git diff --quiet imgs/parrot_blur.jpeg
```


### Grayscale and Blur Combined

```scrut
$ ./flatcv imgs/parrot.jpeg grayscale, blur 9 imgs/parrot_grayscale_blur.jpeg
Loaded image: 512x512 with 3 channels
Executing pipeline with 2 operations:
Applying operation: grayscale
  → Completed in \d+.\d+ ms \(output: 512x512\) (regex)
Applying operation: blur with parameter: 9.00
  → Completed in \d+.\d+ ms \(output: 512x512\) (regex)
Final output dimensions: 512x512
Successfully saved processed image to 'imgs/parrot_grayscale_blur.jpeg'
```

```scrut
$ git diff --quiet imgs/parrot_grayscale_blur.jpeg
```

## Page Image

### Smart Black & White Conversion

```scrut
$ ./flatcv imgs/page.png bw_smart imgs/page_bw_smart.png
Loaded image: 384x256 with 4 channels
Executing pipeline with 1 operations:
Applying operation: bw_smart
  → Completed in \d+.\d+ ms \(output: 384x256\) (regex)
Final output dimensions: 384x256
Successfully saved processed image to 'imgs/page_bw_smart.png'
```

```scrut
$ git diff --quiet imgs/page_bw_smart.png
```


### Smooth (Anti-Aliased) and Smart Black & White Conversion

```scrut
$ ./flatcv imgs/page.png bw_smooth imgs/page_bw_smooth.png
Loaded image: 384x256 with 4 channels
Executing pipeline with 1 operations:
Applying operation: bw_smooth
  → Completed in \d+.\d+ ms \(output: 384x256\) (regex)
Final output dimensions: 384x256
Successfully saved processed image to 'imgs/page_bw_smooth.png'
```

```scrut
$ git diff --quiet imgs/page_bw_smooth.png
```

## Resize Operations

### Percentage Resize (Half Size)

```scrut
$ ./flatcv imgs/parrot.jpeg "resize 50%" imgs/parrot_resize_50_percent.jpeg
Loaded image: 512x512 with 3 channels
Executing pipeline with 1 operations:
Applying operation: resize with parameter: 50%
  → Completed in \d+.\d+ ms \(output: 256x256\) (regex)
Final output dimensions: 256x256
Successfully saved processed image to 'imgs/parrot_resize_50_percent.jpeg'
```

### Mixed Percentage Resize

```scrut
$ ./flatcv imgs/parrot.jpeg "resize 50%x200%" imgs/parrot_resize_50x200_percent.jpeg
Loaded image: 512x512 with 3 channels
Executing pipeline with 1 operations:
Applying operation: resize with parameter: 50%x200%
  → Completed in \d+.\d+ ms \(output: 256x1024\) (regex)
Final output dimensions: 256x1024
Successfully saved processed image to 'imgs/parrot_resize_50x200_percent.jpeg'
```

### Absolute Size Resize

```scrut
$ ./flatcv imgs/parrot.jpeg "resize 800x600" imgs/parrot_resize_800x600.jpeg
Loaded image: 512x512 with 3 channels
Executing pipeline with 1 operations:
Applying operation: resize with parameter: 800x600
  → Completed in \d+.\d+ ms \(output: 800x600\) (regex)
Final output dimensions: 800x600
Successfully saved processed image to 'imgs/parrot_resize_800x600.jpeg'
```

### Resize Combined with Other Operations

```scrut
$ ./flatcv imgs/parrot.jpeg "grayscale, resize 50%, blur 2" imgs/parrot_gray_resize_blur.jpeg
Loaded image: 512x512 with 3 channels
Executing pipeline with 3 operations:
Applying operation: grayscale
  → Completed in \d+.\d+ ms \(output: 512x512\) (regex)
Applying operation: resize with parameter: 50%
  → Completed in \d+.\d+ ms \(output: 256x256\) (regex)
Applying operation: blur with parameter: 2.00
  → Completed in \d+.\d+ ms \(output: 256x256\) (regex)
Final output dimensions: 256x256
Successfully saved processed image to 'imgs/parrot_gray_resize_blur.jpeg'
```
