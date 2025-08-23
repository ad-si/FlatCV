# FlatCV

Image processing and computer vision library in pure C.

<table>
  <tr>
    <td><img src=imgs/elevation_2_basins_receipt.png width=500></td>
    <td><img src=imgs/receipt_corners.png width=500/></td>
    <td><img src=imgs/elevation_2_basins_receipt_watershed.png width=500></td>
  </tr>
</table>

Check out the sidebar on the left for all available features.


### Characteristics

- Color images are a flat array of 8-bit per channel RGBA row-major top-to-bottom
- Grayscale images are a flat array of 8-bit GRAY row-major top-to-bottom
- All operations are done on the raw sRGB pixel values
- Available as an amalgamation where all code is combined into one file. \
    (Each [release](https://github.com/ad-si/FlatCV/releases)
    includes a `flatcv.h` and `flatcv.c` file.)
- Simple code
  - Only uses functions from the C standard library.
  - Minimal usage of macros and preprocessor
  - No multithreading \
      Yet, it's still often faster than GraphicsMagick with multiple threads.
      (See [benchmark](#benchmark) below.)
      For batch processing, make sure to process one file per core.
  - No GPU acceleration
  - No fusing of image transformations


### Playground

You can try out FlatCV in your browser
using the [FlatCV Playground](playground/index.html).
It uses [WebAssembly](https://webassembly.org)
to run the image processing code in your browser.


### Code

You can find the ISC licensed source code
[on GitHub](https://github.com/ad-si/FlatCV).


### Examples

All examples in this documentation are executed during testing.
So you can be confident that they work as described!
