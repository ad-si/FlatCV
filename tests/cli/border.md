# Border

Add borders around images.


## Black Border

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_border_black.jpeg)

```scrut
$ ./flatcv imgs/parrot.jpeg border 000000 10 imgs/parrot_border_black.jpeg
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: border with parameter: 000000
  → Completed in \d+.\d+ ms \(output: 532x404\) (regex)
Final output dimensions: 532x404
Successfully saved processed image to 'imgs/parrot_border_black.jpeg'
```


## Colored Border

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_border_colored.jpeg)

```scrut
$ ./flatcv imgs/parrot.jpeg border E392A7 15 imgs/parrot_border_colored.jpeg
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: border with parameter: E392A7
  → Completed in \d+.\d+ ms \(output: 542x414\) (regex)
Final output dimensions: 542x414
Successfully saved processed image to 'imgs/parrot_border_colored.jpeg'
```
