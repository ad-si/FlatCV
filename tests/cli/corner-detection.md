## Corner Detection

### Detect Corners

```scrut
$ ./flatcv imgs/receipt.jpeg detect_corners
Loaded image: 1024x1024 with 3 channels
Executing pipeline with 1 operations:
Applying operation: detect_corners
  {
    "corners": {
      "top_left": [332, 68],
      "top_right": [692, 76],
      "bottom_right": [720, 956],
      "bottom_left": [352, 960]
    }
  }
  → Completed in \d+.\d+ ms \(output: 1024x1024\) (regex)
```


### Draw Corners

Input | Output
------|--------
![](imgs/receipt.jpeg) | ![](imgs/receipt_corners.png)

```scrut
$ ./flatcv imgs/receipt.jpeg draw_corners imgs/receipt_corners.png
Loaded image: 1024x1024 with 3 channels
Executing pipeline with 1 operations:
Applying operation: draw_corners
  Detected corners:
    Top-left:     (332, 68)
    Top-right:    (692, 76)
    Bottom-right: (720, 956)
    Bottom-left:  (352, 960)
  → Completed in \d+.\d+ ms \(output: 1024x1024\) (regex)
Final output dimensions: 1024x1024
Successfully saved processed image to 'imgs/receipt_corners.png'
```
