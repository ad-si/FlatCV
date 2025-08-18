## Usage

FlatCV can either be used via its CLI or as a C library.


### CLI

The CLI supports edit pipelines which sequentially apply all transformations.

```sh
flatcv <input> <comma-separated-edit-pipeline> <output>
```

As commas are not special characters in shells,
you can write the edit pipeline without quotes.
Both variants yield the same result:

```sh
flatcv i.jpg 'grayscale, blur 9' o.jpg
flatcv i.jpg grayscale, blur 9 o.jpg
```

Input | Output
------|--------
![](imgs/parrot.jpeg) | ![](imgs/parrot_grayscale_blur.jpeg)

```scrut
$ ./flatcv imgs/parrot.jpeg grayscale, blur 9 imgs/parrot_grayscale_blur.jpeg
Loaded image: 512x384 with 3 channels
Executing pipeline with 2 operations:
Applying operation: grayscale
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Applying operation: blur with parameter: 9.00
  → Completed in \d+.\d+ ms \(output: 512x384\) (regex)
Final output dimensions: 512x384
Successfully saved processed image to 'imgs/parrot_grayscale_blur.jpeg'
```

```scrut
$ git diff --quiet imgs/parrot_grayscale_blur.jpeg
```


### Library

```c
#include "flatcv.h"

// Resize an image to half size
unsigned char const * half_size = resize(
    input_width, input_height,
    0.5, 0.5,
    &out_width, &out_height,
    input_data
);

// Do something with the resized image

// Free the allocated memory
free(half_size);
```
