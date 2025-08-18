## Segmentation

### Watershed

#### Watershed with 2 Markers for 1 White Ring

Input | Output
------|--------
![](imgs/elevation_2_basins_1_ring.png) | ![](imgs/elevation_2_basins_1_ring_watershed.png)

The image is a white ring in the center of a black background.
Running the following watershed segmentation command segments the image into 2 regions:
- Disk in the center with a radius that goes to the middle of the rings edge
- The rest of the image

```scrut
$ ./flatcv imgs/elevation_2_basins_1_ring.png watershed 0x0 150x100 imgs/elevation_2_basins_1_ring_watershed.png
Loaded image: 300x200 with 1 channels
Executing pipeline with 1 operations:
Applying operation: watershed with parameter: 0x0 150x100
  → Completed in \d+.\d+ ms \(output: 300x200\) (regex)
Final output dimensions: 300x200
Successfully saved processed image to 'imgs/elevation_2_basins_1_ring_watershed.png'
```


#### Watershed with 2 Markers for 2 White Rings

Input | Output
------|--------
![](imgs/elevation_2_basins_2_rings.png) | ![](imgs/elevation_2_basins_2_rings_watershed.png)

The image contains 2 white rings in the center of a black background.
Running the following watershed segmentation command segments the image into 2 regions:
- Disk in the center with a radius that goes to the middle of the black gap between the rings
- The rest of the image

```scrut
$ ./flatcv imgs/elevation_2_basins_2_rings.png watershed 0x0 150x100 imgs/elevation_2_basins_2_rings_watershed.png
Loaded image: 300x200 with 1 channels
Executing pipeline with 1 operations:
Applying operation: watershed with parameter: 0x0 150x100
  → Completed in \d+.\d+ ms \(output: 300x200\) (regex)
Final output dimensions: 300x200
Successfully saved processed image to 'imgs/elevation_2_basins_2_rings_watershed.png'
```


#### Watershed with 2 Markers for a Receipt

Input | Output
------|--------
![](imgs/elevation_2_basins_receipt.png) | ![](imgs/elevation_2_basins_receipt_watershed.png)

The image contains the elevation map of a photo of a receipt.
Running the following watershed segmentation command segments the image into 2 regions:
- The receipt itself
- The background

```scrut
$ ./flatcv imgs/elevation_2_basins_receipt.png watershed 0x0 128x128 imgs/elevation_2_basins_receipt_watershed.png
Loaded image: 256x256 with 1 channels
Executing pipeline with 1 operations:
Applying operation: watershed with parameter: 0x0 128x128
  → Completed in \d+.\d+ ms \(output: 256x256\) (regex)
Final output dimensions: 256x256
Successfully saved processed image to 'imgs/elevation_2_basins_receipt_watershed.png'
```


#### Watershed with 3 Markers

Input | Output
------|--------
![](imgs/elevation_3_basins_gradient.png) | ![](imgs/elevation_3_basins_gradient_watershed.png)

The image is a horizontal gradient from white to black and back to white.
There is also a white disk with black edge in the center.
Running the following watershed segmentation command segments the image into three regions:
- Left side
- Center
- Right side

```scrut
$ ./flatcv imgs/elevation_3_basins_gradient.png watershed 0x0 150x100 299x0 imgs/elevation_3_basins_gradient_watershed.png
Loaded image: 300x200 with 1 channels
Executing pipeline with 1 operations:
Applying operation: watershed with parameter: 0x0 150x100 299x0
  → Completed in \d+.\d+ ms \(output: 300x200\) (regex)
Final output dimensions: 300x200
Successfully saved processed image to 'imgs/elevation_3_basins_gradient_watershed.png'
```
