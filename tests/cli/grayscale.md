## Grayscale Conversion

Convert color images to grayscale using the luminosity method, which weighs RGB channels based on human perception of brightness.


### Basic Grayscale

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_grayscale.jpeg)

```scrut
$ ./flatcv imgs/parrot.jpeg grayscale imgs/parrot_grayscale.jpeg
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: grayscale
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_grayscale.jpeg'
```

```scrut
$ git diff --quiet imgs/parrot_grayscale.jpeg
```
