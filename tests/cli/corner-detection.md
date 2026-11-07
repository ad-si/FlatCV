## Corner Detection

### Detect Corners

```scrut
$ ./flatcv imgs/receipt.jpeg detect_corners
Loaded image: 1024x1024 with 3 channels
Executing pipeline with 1 operations:
Applying operation: detect_corners
  → Completed in \d+.\d+ ms \(output: 1024x1024\) (regex)
  {
    "corners": {
      "top_left": [332, 68],
      "top_right": [692, 76],
      "bottom_right": [720, 956],
      "bottom_left": [352, 960]
    }
  }
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
  → Completed in \d+.\d+ ms \(output: 1024x1024\) (regex)
Final output dimensions: 1024x1024
Successfully saved processed image to 'imgs/receipt_corners.png'
  Detected corners:
    Top-left:     (332, 68)
    Top-right:    (692, 76)
    Bottom-right: (720, 956)
    Bottom-left:  (352, 960)
```


### Keep a skewed landscape receipt upright

This synthetic receipt has a lower-left corner with a smaller raw `x + y`
than its upper-left corner. Corner labels must follow the document's bounds
so extraction does not introduce a quarter-turn. The fixture is a 720×540
image with background RGB 30 and a solid RGB 240 quadrilateral at
(261,215), (703,219), (692,367), (14,361).

```scrut
$ ./flatcv tests/skewed-receipt.png detect_corners
Loaded image: 720x540 with 3 channels
Executing pipeline with 1 operations:
Applying operation: detect_corners
  → Completed in \d+.\d+ ms \(output: 720x540\) (regex)
  {
    "corners": {
      "top_left": [259, 219],
      "top_right": [700, 219],
      "bottom_right": [689, 363],
      "bottom_left": [31, 356]
    }
  }
```
