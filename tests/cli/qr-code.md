## QR Code Decoding

Detect and decode QR codes in an image.
Outputs the decoded text along with the bounding corners
and finder pattern centers as JSON.


### Decode

```scrut
$ ./flatcv imgs/qr_hello.jpeg qr
Loaded image: 928x928 with 1 channels
Executing pipeline with 1 operations:
Applying operation: qr
  → Completed in \d+.\d+ ms \(output: 928x928\) (regex)
  {
    "qr_codes": [
      {
        "text": "Hello World!",
        "corners": {
          "top_left": [63.5, 63.5],
          "top_right": [863.5, 63.5],
          "bottom_right": [863.5, 863.5],
          "bottom_left": [63.5, 863.5]
        },
        "finders": {
          "top_left": [175.5, 175.5],
          "top_right": [751.5, 175.5],
          "bottom_left": [175.5, 751.5]
        },
        "module_size": 32.00
      }
    ]
  }
```


### Draw Detected QR Codes

Draw every feature FlatCV detected for each decoded QR:

- Green polygon around the outer border
- Red outlines and filled dots at the three finder pattern centers
- Magenta outlines and dots at every alignment pattern
- Yellow dots along the two timing patterns

Marker sizes are scaled to the QR code's module size.

```scrut
$ ./flatcv imgs/qr_hello.jpeg qr_draw imgs/qr_hello_annotated.png
Loaded image: 928x928 with 1 channels
Executing pipeline with 1 operations:
Applying operation: qr_draw
  → Completed in \d+.\d+ ms \(output: 928x928\) (regex)
Final output dimensions: 928x928
Successfully saved processed image to 'imgs/qr_hello_annotated.png'
  QR code 1: v2 (25x25 modules), 1 alignment pattern: "Hello World!"
```


### Pipeline

The decoder works as follows:

1. **Finder pattern detection** — Scan rows, columns, and both diagonals
   for the characteristic 1:1:3:1:1 dark/light ratio, cross-check at
   multiple angles, and merge nearby detections.
2. **Pattern selection** — When more than 3 candidates are found, rank all
   triples by module-size consistency and right-angle geometry.
3. **Orientation identification** — The top-left finder is the vertex
   opposite the hypotenuse; a cross product distinguishes TR from BL.
4. **Grid sampling** — An affine transform maps finder centers to their
   known module coordinates (3.5, 3.5 from each corner), then samples
   every module center.
5. **Format decoding** — Both copies of the 15-bit format string are
   matched against all 32 BCH-encoded values, tolerating up to 3 bit
   errors, yielding the error correction level and mask pattern.
6. **Data extraction** — Bits are read in the zigzag column-pair traversal,
   skipping all function patterns (finders, separators, timing, alignment,
   format/version info). The mask is applied to recover the raw stream.
7. **Codeword de-interleaving** — For multi-block EC configurations, the
   interleaved stream is split back into individual blocks.
8. **Payload decoding** — The stream is parsed per the QR spec:
   mode indicator, character count, then encoded data. Supported modes:
   - Byte (mode 4) — 8 bits per character
   - Alphanumeric (mode 2) — pairs in 11 bits, remainder in 6
   - Numeric (mode 1) — triples in 10 bits, with 7-bit and 4-bit remainders


### Related

- [Quirc] - QR decoder library written in C.
- [Rqrr] - QR reader written in Rust.
- [Rxing] - Rust port of the ZXing multi-format barcode library.
- [ZBar] - C library for reading bar codes from images and video streams.
- [Zedbar] - Rust library for reading bar codes from images based on ZBar.

[Quirc]: https://github.com/dlbeer/quirc
[Rqrr]: https://github.com/WanzenBug/rqrr
[Rxing]: https://github.com/rxing-core/rxing
[ZBar]: https://github.com/mchehab/zbar
[Zedbar]: https://github.com/eventualbuddha/zedbar
