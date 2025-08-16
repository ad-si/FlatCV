# FlatCV

Simple image processing and computer vision library in pure C.

"Simple" means:

- Color images are a flat array of 8-bit per channel RGBA row-major top-to-bottom
- Grayscale images are a flat array of 8-bit GRAY row-major top-to-bottom
- All operations are done on the raw sRGB pixel values
- Minimal usage of macros and preprocessor
- Available as an amalgamation where all code is combined into one file.
    (Each [release](https://github.com/ad-si/FlatCV/releases)
    includes a `flatcv.h` and `flatcv.c` file.)
- No fusing of image transformations
- No multithreading \
    You're more likely to process one file per core
    than one file over multiple cores anyways.
    Yet, it's still often faster than GraphicsMagick with multiple threads.
    (See [benchmark](#benchmark) below.)
- No GPU acceleration
- Only uses functions from the C standard library.


## Usage

### CLI

FlatCV can either be used as a C library or via its CLI.
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


#### Examples

Check out the [full documentation](./tests/cli.md)
for more examples and usage instructions.

Command | Input | Output
--------|-------|--------
`flatcv i.jpg grayscale o.jpg` | ![Parrot](./imgs/parrot.jpeg) | ![Parrot Grayscale](./imgs/parrot_grayscale.jpeg)
`flatcv i.jpg grayscale, blur 9 o.jpg` | ![Parrot](./imgs/parrot.jpeg) | ![Parrot Grayscale and Blur](./imgs/parrot_grayscale_blur.jpeg)
`flatcv i.jpg bw_smooth o.jpg` | ![Parrot](./imgs/page.png) | ![Smooth Binarization](./imgs/page_bw_smooth.png)
`flatcv i.jpg watershed 0x0 300x200 599x0 o.jpg` | ![Parrot](./imgs/elevation_3_basins_gradient.png) | ![Watershed Segmentation](./imgs/elevation_3_basins_gradient_watershed.png)


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


## Benchmark

```txt
===== ▶️ Resize =====

./flatcv imgs/parrot_hq.jpeg resize 256x256 tmp/resize_flatcv.png ran
1.42 ± 0.01 times faster than gm convert imgs/parrot_hq.jpeg -resize 256x256! tmp/resize_gm.png
2.39 ± 0.05 times faster than magick convert imgs/parrot_hq.jpeg -resize 256x256! tmp/resize_magick.png

===== ▶️ Crop =====

gm convert imgs/parrot_hq.jpeg -crop 100x100+200+200 tmp/crop_gm.png ran
1.30 ± 0.04 times faster than magick convert imgs/parrot_hq.jpeg -crop 100x100+200+200 tmp/crop_magick.png
1.30 ± 0.04 times faster than ./flatcv imgs/parrot_hq.jpeg crop 100 100 100 100 tmp/crop_flatcv.png

===== ▶️ Grayscale =====

gm convert imgs/village.jpeg -colorspace Gray tmp/gray_gm.png ran
1.76 ± 0.02 times faster than ./flatcv imgs/village.jpeg grayscale tmp/gray_flatcv.png
3.83 ± 0.04 times faster than magick convert imgs/village.jpeg -colorspace Gray tmp/gray_magick.png

===== ▶️ Blur =====

./flatcv imgs/page_hq.png blur 21 tmp/blur_flatcv.png ran
1.66 ± 0.01 times faster than gm convert imgs/page_hq.png -blur 21x7 tmp/blur_gm.png
2.20 ± 0.01 times faster than magick convert imgs/page_hq.png -blur 21x7 tmp/blur_magick.png

===== ▶️ Sobel =====

./flatcv imgs/page_hq.png sobel tmp/sobel_flatcv.png ran
3.07 ± 0.05 times faster than magick convert imgs/page_hq.png -define convolve:scale="!" \
        -define morphology:compose=Lighten \
        -morphology Convolve "Sobel:>" tmp/sobel_magick.png
```


## FAQ

### Why is this written in C?

- **Most portable language** \
    Almost every other language has support to integrate C code
    in one way or another.
    Especially since it's available a single file
    that can be vendored into your project.

- **Lot of existing code** \
    C and C++ are the most widely used languages for image processing.
    So there is a lot of existing code that can be reused and adapted.

- **Great for AI powered development** \
    As there is so much training data available,
    LLMs are especially good at writing C code.


### Will you rewrite this in another language?

I am open to rewrite this in [Rust](https://www.rust-lang.org)
or [Zig](https://ziglang.org) in the future,
once the advantages of C become less relevant.
