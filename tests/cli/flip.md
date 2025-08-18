## Image Flipping

### Horizontal Flip (flip_x)

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_flip_x.png)

```scrut
$ ./flatcv imgs/parrot.jpeg flip_x imgs/parrot_flip_x.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: flip_x
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_flip_x.png'
```


### Vertical Flip (flip_y)

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_flip_y.png)

```scrut
$ ./flatcv imgs/parrot.jpeg flip_y imgs/parrot_flip_y.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: flip_y
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_flip_y.png'
```


### Both Flips Combined (180° Rotation)

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_flip_both.png)

```scrut
$ ./flatcv imgs/parrot.jpeg flip_x, flip_y imgs/parrot_flip_both.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 2 operations:
Applying operation: flip_x
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Applying operation: flip_y
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_flip_both.png'
```
