## Image Rotation

### Rotate 90 Degrees Clockwise

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_rotate_90.png)

```scrut
$ ./flatcv imgs/parrot.jpeg "rotate 90" imgs/parrot_rotate_90.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: rotate with parameter: 90.0+ (regex)
  → Completed in \d+.\d+ ms \(output: 384x512\) (regex)
Final output dimensions: 384x512
Successfully saved processed image to 'imgs/parrot_rotate_90.png'
```


### Rotate 180 Degrees

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_rotate_180.png)

```scrut
$ ./flatcv imgs/parrot.jpeg "rotate 180" imgs/parrot_rotate_180.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: rotate with parameter: 180.0+ (regex)
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_rotate_180.png'
```


### Rotate 270 Degrees Clockwise (90 Counter-Clockwise)

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_rotate_270.png)

```scrut
$ ./flatcv imgs/parrot.jpeg "rotate 270" imgs/parrot_rotate_270.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: rotate with parameter: 270.0+ (regex)
  → Completed in \d+.\d+ ms \(output: 384x512\) (regex)
Final output dimensions: 384x512
Successfully saved processed image to 'imgs/parrot_rotate_270.png'
```


### Rotate with Negative Angle (-90 = 270)

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_rotate_neg90.png)

```scrut
$ ./flatcv imgs/parrot.jpeg "rotate -90" imgs/parrot_rotate_neg90.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: rotate with parameter: -90.0+ (regex)
  → Completed in \d+.\d+ ms \(output: 384x512\) (regex)
Final output dimensions: 384x512
Successfully saved processed image to 'imgs/parrot_rotate_neg90.png'
```


### Chained Rotations

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_rotate_chain.png)

```scrut
$ ./flatcv imgs/parrot.jpeg "rotate 90, rotate 90" imgs/parrot_rotate_chain.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 2 operations:
Applying operation: rotate with parameter: 90.0+ (regex)
  → Completed in \d+.\d+ ms \(output: 384x512\) (regex)
Applying operation: rotate with parameter: 90.0+ (regex)
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_rotate_chain.png'
```


### Invalid Angle (Not Multiple of 90)

```scrut
$ ./flatcv imgs/parrot.jpeg "rotate 45" imgs/parrot_rotate_invalid.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: rotate with parameter: 45.0+ (regex)
Error: rotate angle must be a multiple of 90
  → Completed in 0.0 ms (output: 512x384)
Error: Failed to execute pipeline
[1]
```
