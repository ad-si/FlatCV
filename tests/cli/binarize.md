## Binarize

Convert images to black and white.

Process:

1. Convert the image to grayscale.
1. Remove low frequencies (e.g. shadows)
1. Apply threshold with [OTSU's method](https://en.wikipedia.org/wiki/Otsu%27s_method)


### Smart Black & White

Input | Output
------|--------
![](imgs/page.png) | ![](imgs/page_bw_smart.png)

```scrut
$ ./flatcv imgs/page.png bw_smart imgs/page_bw_smart.png
Loaded image: 384x256 with 1 channels
Executing pipeline with 1 operations:
Applying operation: bw_smart
  → Completed in \d+.\d+ ms \(output: 384x256\) (regex)
Final output dimensions: 384x256
Successfully saved processed image to 'imgs/page_bw_smart.png'
```

```scrut
$ git diff --quiet imgs/page_bw_smart.png
```


### Smart and Smooth (Anti-Aliased) Black & White

Input | Output
------|--------
![](imgs/page.png) | ![](imgs/page_bw_smooth.png)

```scrut
$ ./flatcv imgs/page.png bw_smooth imgs/page_bw_smooth.png
Loaded image: 384x256 with 1 channels
Executing pipeline with 1 operations:
Applying operation: bw_smooth
  → Completed in \d+.\d+ ms \(output: 384x256\) (regex)
Final output dimensions: 384x256
Successfully saved processed image to 'imgs/page_bw_smooth.png'
```

```scrut
$ git diff --quiet imgs/page_bw_smooth.png
```


### Otsu Threshold

Apply [Otsu's method](https://en.wikipedia.org/wiki/Otsu%27s_method) to automatically find the optimal threshold value for binarization.

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_threshold.png)

```scrut
$ ./flatcv imgs/parrot.jpeg threshold imgs/parrot_threshold.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: threshold
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_threshold.png'
```


### Sobel Edge Detection

Detect edges in an image using the [Sobel operator](https://en.wikipedia.org/wiki/Sobel_operator), which calculates the gradient of image intensity at each pixel.

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_sobel.png)

```scrut
$ ./flatcv imgs/parrot.jpeg sobel imgs/parrot_sobel.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: sobel
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_sobel.png'
```
