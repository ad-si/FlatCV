## Crop

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_crop.jpeg)

```scrut
$ ./flatcv imgs/parrot.jpeg crop 200x150+100+100 imgs/parrot_crop.jpeg
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: crop with parameter: 100.00 100.00 200.00 150.00
  → Completed in \d+.\d+ ms \(output: 200x150\) (regex)
Final output dimensions: 200x150
Successfully saved processed image to 'imgs/parrot_crop.jpeg'
```
