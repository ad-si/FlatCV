# CLI Tests

## Parrot Image

### Grayscale Conversion

```scrut
$ ./flatcv imgs/parrot.jpeg grayscale imgs/parrot_grayscale.jpeg
Loaded image: 512x512 with 3 channels
Executing pipeline with 1 operations:
Applying operation: grayscale
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
Applying operation: blur with parameter: 9.00
Successfully saved processed image to 'imgs/parrot_grayscale_blur.jpeg'
```

```scrut
$ git diff --quiet imgs/parrot_grayscale_blur.jpeg
```

## Page Image

### Smart Black and White Conversion

```scrut
$ ./flatcv imgs/page.png bw_smart imgs/page_bw_smart.png
Loaded image: 1536x1024 with 3 channels
Executing pipeline with 1 operations:
Applying operation: bw_smart
Successfully saved processed image to 'imgs/page_bw_smart.png'
```

```scrut
$ git diff --quiet imgs/page_bw_smart.png
```
