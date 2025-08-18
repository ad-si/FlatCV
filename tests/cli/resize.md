## Resize

FlatCV uses area-based sampling for shrinking images
and bilinear interpolation for enlarging images.
This ensures a good balance between performance and quality.


### Percentage Resize (Half Size)

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_resize_50_percent.jpeg)

```scrut
$ ./flatcv imgs/parrot.jpeg resize 50% imgs/parrot_resize_50_percent.jpeg
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: resize with parameter: 50%
  → Completed in \d+.\d+ ms \(output: 256x192\) (regex)
Final output dimensions: 256x192
Successfully saved processed image to 'imgs/parrot_resize_50_percent.jpeg'
```


### Mixed Percentage Resize

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_resize_50x150_percent.jpeg)

```scrut
$ ./flatcv imgs/parrot.jpeg resize 50%x150% imgs/parrot_resize_50x150_percent.jpeg
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: resize with parameter: 50%x150%
  → Completed in \d+.\d+ ms \(output: 256x576\) (regex)
Final output dimensions: 256x576
Successfully saved processed image to 'imgs/parrot_resize_50x150_percent.jpeg'
```


### Absolute Size Resize

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_resize_800x400.jpeg)

```scrut
$ ./flatcv imgs/parrot.jpeg resize 800x400 imgs/parrot_resize_800x400.jpeg
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: resize with parameter: 800x400
  → Completed in \d+.\d+ ms \(output: 800x400\) (regex)
Final output dimensions: 800x400
Successfully saved processed image to 'imgs/parrot_resize_800x400.jpeg'
```


### Resize Combined with Other Operations

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_gray_resize_blur.jpeg)

```scrut
$ ./flatcv imgs/parrot.jpeg grayscale, resize 50%, blur 2 imgs/parrot_gray_resize_blur.jpeg
Loaded image: 512x384 with 3 channels
Executing pipeline with 3 operations:
Applying operation: grayscale
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Applying operation: resize with parameter: 50%
  → Completed in \d+.\d+ ms \(output: 256x192\) (regex)
Applying operation: blur with parameter: 2.00
  → Completed in \d+.\d+ ms \(output: 256x192\) (regex)
Final output dimensions: 256x192
Successfully saved processed image to 'imgs/parrot_gray_resize_blur.jpeg'
```
