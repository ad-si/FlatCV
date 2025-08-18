## Draw

### Circle

#### Single Red Circle

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_circle_red.png)

```scrut
$ ./flatcv imgs/parrot.jpeg circle FF0000 50 200x150 imgs/parrot_circle_red.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: circle with parameter: FF0000 200.00 150.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_circle_red.png'
```


#### Single Blue Circle

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_circle_blue.png)

```scrut
$ ./flatcv imgs/parrot.jpeg circle 0000FF 30 100x100 imgs/parrot_circle_blue.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: circle with parameter: 0000FF 100.00 100.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_circle_blue.png'
```


#### Multiple Colored Circles

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_circles_multi.png)

```scrut
$ ./flatcv imgs/parrot.jpeg circle 00FF00 25 400x200, circle FFFF00 35 100x200 imgs/parrot_circles_multi.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 2 operations:
Applying operation: circle with parameter: 00FF00 400.00 200.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Applying operation: circle with parameter: FFFF00 100.00 200.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_circles_multi.png'
```


#### Circle with Grayscale Conversion

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_gray_circle.png)

```scrut
$ ./flatcv imgs/parrot.jpeg grayscale, circle FFFFFF 40 250x250 imgs/parrot_gray_circle.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 2 operations:
Applying operation: grayscale
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Applying operation: circle with parameter: FFFFFF 250.00 250.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_gray_circle.png'
```


#### Circle at Image Boundary

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_circle_boundary.png)

```scrut
$ ./flatcv imgs/parrot.jpeg circle 00FFFF 80 10x10 imgs/parrot_circle_boundary.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: circle with parameter: 00FFFF 10.00 10.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_circle_boundary.png'
```


### Disk

#### Single Red Disk

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_disk_red.png)

```scrut
$ ./flatcv imgs/parrot.jpeg disk FF0000 50 200x150 imgs/parrot_disk_red.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: disk with parameter: FF0000 200.00 150.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_disk_red.png'
```


#### Single Blue Disk

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_disk_blue.png)

```scrut
$ ./flatcv imgs/parrot.jpeg disk 0000FF 30 100x100 imgs/parrot_disk_blue.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: disk with parameter: 0000FF 100.00 100.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_disk_blue.png'
```


#### Multiple Colored Disks

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_disks_multi.png)

```scrut
$ ./flatcv imgs/parrot.jpeg disk 00FF00 25 400x200, disk FFFF00 35 100x200 imgs/parrot_disks_multi.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 2 operations:
Applying operation: disk with parameter: 00FF00 400.00 200.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Applying operation: disk with parameter: FFFF00 100.00 200.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_disks_multi.png'
```


#### Disk with Grayscale Conversion

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_gray_disk.png)

```scrut
$ ./flatcv imgs/parrot.jpeg grayscale, disk FFFFFF 40 250x250 imgs/parrot_gray_disk.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 2 operations:
Applying operation: grayscale
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Applying operation: disk with parameter: FFFFFF 250.00 250.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_gray_disk.png'
```


#### Disk at Image Boundary

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_disk_boundary.png)

```scrut
$ ./flatcv imgs/parrot.jpeg disk 00FFFF 80 10x10 imgs/parrot_disk_boundary.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 1 operations:
Applying operation: disk with parameter: 00FFFF 10.00 10.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_disk_boundary.png'
```


#### Circle and Disk Combined

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_circle_disk_combo.png)

```scrut
$ ./flatcv imgs/parrot.jpeg disk FF0000 60 250x250, circle FFFFFF 65 250x250 imgs/parrot_circle_disk_combo.png
Loaded image: 512x384 with 3 channels
Executing pipeline with 2 operations:
Applying operation: disk with parameter: FF0000 250.00 250.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Applying operation: circle with parameter: FFFFFF 250.00 250.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_circle_disk_combo.png'
```
