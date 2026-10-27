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


### Flood Fill

Usage: `flood_fill <threshold> <value> <x1>x<y1> [<x2>x<y2> ...]`

Starting from each seed point,
all connected pixels whose grayscale value differs
by at most `threshold` from the seed's value
are set to the gray `value`.
Each seed is filled independently relative to its own value.
`flood_fill` uses 4-connectivity (edge neighbors only),
`flood_fill_8` uses 8-connectivity (includes diagonal neighbors).


#### Fill the Background

Input | Output
------|--------
![](imgs/elevation_2_basins_1_ring.png) | ![](imgs/elevation_2_basins_1_ring_flood_fill.png)

```scrut
$ ./flatcv imgs/elevation_2_basins_1_ring.png flood_fill 10 128 0x0 imgs/elevation_2_basins_1_ring_flood_fill.png
Loaded image: 300x200 with 1 channels
Executing pipeline with 1 operations:
Applying operation: flood_fill with parameter: 10 128 0x0
  → Completed in \d+.\d+ ms \(output: 300x200\) (regex)
Final output dimensions: 300x200
Successfully saved processed image to 'imgs/elevation_2_basins_1_ring_flood_fill.png'
```


#### Multiple Seeds

The background and the disk in the center of the ring
are separate regions and need one seed each.

Input | Output
------|--------
![](imgs/elevation_2_basins_1_ring.png) | ![](imgs/elevation_2_basins_1_ring_flood_fill_2_seeds.png)

```scrut
$ ./flatcv imgs/elevation_2_basins_1_ring.png flood_fill 10 128 0x0 150x100 imgs/elevation_2_basins_1_ring_flood_fill_2_seeds.png
Loaded image: 300x200 with 1 channels
Executing pipeline with 1 operations:
Applying operation: flood_fill with parameter: 10 128 0x0 150x100
  → Completed in \d+.\d+ ms \(output: 300x200\) (regex)
Final output dimensions: 300x200
Successfully saved processed image to 'imgs/elevation_2_basins_1_ring_flood_fill_2_seeds.png'
```


#### 4-Connectivity vs 8-Connectivity

The diamond's outline is 1 pixel wide and consists only of diagonal steps.
It encloses the region for 4-connected filling,
but 8-connected filling leaks through the diagonal gaps.

Input | `flood_fill` | `flood_fill_8`
------|--------------|---------------
![](imgs/diamond_outline.png) | ![](imgs/diamond_outline_flood_fill.png) | ![](imgs/diamond_outline_flood_fill_8.png)

```scrut
$ ./flatcv imgs/diamond_outline.png flood_fill 0 128 100x100 imgs/diamond_outline_flood_fill.png
Loaded image: 200x200 with 1 channels
Executing pipeline with 1 operations:
Applying operation: flood_fill with parameter: 0 128 100x100
  → Completed in \d+.\d+ ms \(output: 200x200\) (regex)
Final output dimensions: 200x200
Successfully saved processed image to 'imgs/diamond_outline_flood_fill.png'
```

```scrut
$ ./flatcv imgs/diamond_outline.png flood_fill_8 0 128 100x100 imgs/diamond_outline_flood_fill_8.png
Loaded image: 200x200 with 1 channels
Executing pipeline with 1 operations:
Applying operation: flood_fill_8 with parameter: 0 128 100x100
  → Completed in \d+.\d+ ms \(output: 200x200\) (regex)
Final output dimensions: 200x200
Successfully saved processed image to 'imgs/diamond_outline_flood_fill_8.png'
```


#### Seed Outside the Image

```scrut
$ ./flatcv imgs/elevation_2_basins_1_ring.png flood_fill 10 128 300x0 imgs/elevation_2_basins_1_ring_flood_fill_invalid.png
Loaded image: 300x200 with 1 channels
Executing pipeline with 1 operations:
Applying operation: flood_fill with parameter: 10 128 300x0
Error: marker '300x0' is outside the 300x200 image
  → Completed in \d+.\d+ ms \(output: 300x200\) (regex)
Error: Failed to execute pipeline
[1]
```
