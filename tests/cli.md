# CLI Tests

## Grayscale Conversion

```scrut
$ ./flatcv imgs/parrot.jpeg grayscale imgs/grayscale.jpeg && git diff --quiet imgs/grayscale.jpeg
Loaded image: 512x512 with 3 channels
Executing pipeline with 1 operations:
Applying operation: grayscale
Successfully saved processed image to 'imgs/grayscale.jpeg'
```


## Gaussian Blur

```scrut
$ ./flatcv imgs/parrot.jpeg blur 9 imgs/blur.jpeg
Loaded image: 512x512 with 3 channels
Executing pipeline with 1 operations:
Applying operation: blur with parameter: 9.00
Successfully saved processed image to 'imgs/blur.jpeg'
```


## Smart Black and White Conversion

```scrut
$ ./flatcv imgs/parrot.jpeg bw_smart imgs/bw_smart.png
Loaded image: 512x512 with 3 channels
Executing pipeline with 1 operations:
Applying operation: bw_smart
Successfully saved processed image to 'imgs/bw_smart.png'
```


## Grayscale and Blur Combined

```scrut
$ ./flatcv imgs/parrot.jpeg grayscale, blur 9 imgs/grayscale_blur.jpeg
Loaded image: 512x512 with 3 channels
Executing pipeline with 2 operations:
Applying operation: grayscale
Applying operation: blur with parameter: 9.00
Successfully saved processed image to 'imgs/grayscale_blur.jpeg'
```
