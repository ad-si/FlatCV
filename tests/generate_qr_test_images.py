#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["qrcode", "Pillow", "numpy", "zxing-cpp"]
# ///
"""Generate test images with QR codes on varied backgrounds.

Two modes:
  (default) Random mixed-difficulty dataset in tests/qr_codes_generated
  --difficulty Tiered dataset in tests/qr_codes_difficulty/level_N/

The difficulty pipeline has two execution paths:
  --validate    Use zxing-cpp to validate each candidate, retry rejected
                renders, then record the accept/reject pattern to
                tests/qr_codes_difficulty_manifest.json. Needs zxing-cpp
                (same algorithm as pyzxing's ZXing, C++ instead of Java, so
                no JVM startup cost per image).
  (default)     Read the manifest and replay the exact RNG consumption
                sequence the validated run made, so the same images are
                reproduced byte-for-byte without calling any decoder.
                Only needs qrcode + Pillow.

Workflow: run `--validate` once (whenever the generator code changes), commit
the refreshed manifest, then every subsequent regeneration is decoder-free.
"""

import argparse
import io
import json
import math
import random
import string
from pathlib import Path

import numpy as np
import qrcode
from PIL import Image, ImageDraw, ImageFilter


MANIFEST_PATH = Path("tests/qr_codes_difficulty_manifest.json")


CHARSET = string.ascii_letters + string.digits + "_-+"
OUTPUT_DIR = Path("tests/qr_codes_generated")

SIZES = [
    (400, 400),
    (640, 480),
    (800, 600),
    (1024, 768),
    (480, 640),
    (600, 800),
    (1920, 1080),
    (300, 300),
]


def random_text() -> str:
    length = random.randint(1, 30)
    return "".join(random.choice(CHARSET) for _ in range(length))


# Separate numpy RNG for bulk pixel-level draws. Keeps the Python `random`
# stream reserved for the manifest-relevant decisions (text, level params,
# scale, angle, background choice, color picks) — so those stay lockstep
# with the validated manifest even though the pixel fills are vectorised.
_NP_RNG = np.random.default_rng()


def _seed_rngs(seed: int) -> None:
    random.seed(seed)
    global _NP_RNG
    _NP_RNG = np.random.default_rng(seed)


def _paste_array(img: Image.Image, arr: np.ndarray) -> None:
    img.paste(Image.fromarray(arr, "RGB"))


def bg_solid(img: Image.Image) -> None:
    color = tuple(random.randint(128, 255) for _ in range(3))
    img.paste(color, (0, 0, img.width, img.height))


def bg_gradient_horizontal(img: Image.Image) -> None:
    c1 = np.array([random.randint(128, 255) for _ in range(3)], dtype=np.float32)
    c2 = np.array([random.randint(128, 255) for _ in range(3)], dtype=np.float32)
    t = np.linspace(0.0, 1.0, img.width, dtype=np.float32)[None, :, None]
    row = (c1 + (c2 - c1) * t).astype(np.uint8)
    arr = np.broadcast_to(row, (img.height, img.width, 3)).copy()
    _paste_array(img, arr)


def bg_gradient_vertical(img: Image.Image) -> None:
    c1 = np.array([random.randint(128, 255) for _ in range(3)], dtype=np.float32)
    c2 = np.array([random.randint(128, 255) for _ in range(3)], dtype=np.float32)
    t = np.linspace(0.0, 1.0, img.height, dtype=np.float32)[:, None, None]
    col = (c1 + (c2 - c1) * t).astype(np.uint8)
    arr = np.broadcast_to(col, (img.height, img.width, 3)).copy()
    _paste_array(img, arr)


def bg_checkerboard(img: Image.Image) -> None:
    draw = ImageDraw.Draw(img)
    tile = random.randint(20, 60)
    c1 = tuple(random.randint(180, 255) for _ in range(3))
    c2 = tuple(random.randint(180, 255) for _ in range(3))
    for y in range(0, img.height, tile):
        for x in range(0, img.width, tile):
            color = c1 if (x // tile + y // tile) % 2 == 0 else c2
            draw.rectangle([x, y, x + tile, y + tile], fill=color)


def bg_stripes(img: Image.Image) -> None:
    draw = ImageDraw.Draw(img)
    stripe_w = random.randint(10, 40)
    c1 = tuple(random.randint(180, 255) for _ in range(3))
    c2 = tuple(random.randint(180, 255) for _ in range(3))
    for x in range(0, img.width, stripe_w):
        color = c1 if (x // stripe_w) % 2 == 0 else c2
        draw.rectangle([x, 0, x + stripe_w, img.height], fill=color)


def bg_noise(img: Image.Image) -> None:
    arr = _NP_RNG.integers(
        160, 256, size=(img.height, img.width, 3), dtype=np.uint8
    )
    _paste_array(img, arr)


def _value_noise(
    w: int, h: int, base_cells: int, octaves: int, persistence: float = 0.5
) -> np.ndarray:
    """Multi-octave value noise — low-res random grids bilinearly upsampled
    and summed at decreasing amplitude. Returns a (h, w) float32 array in
    [0, 1]. Cheap stand-in for Perlin: no gradients, just smoothed values."""
    out = np.zeros((h, w), dtype=np.float32)
    amp = 1.0
    total = 0.0
    cells = max(2, base_cells)
    for _ in range(octaves):
        grid = _NP_RNG.random((cells, cells), dtype=np.float32)
        layer = np.asarray(
            Image.fromarray((grid * 255.0).astype(np.uint8)).resize(
                (w, h), Image.BILINEAR
            ),
            dtype=np.float32,
        ) / 255.0
        out += layer * amp
        total += amp
        amp *= persistence
        cells *= 2
    return out / total


def bg_photo_like(img: Image.Image) -> None:
    """Large-scale value noise across three channels plus a random base tint —
    broad smooth gradients with organic blotches, reminiscent of out-of-focus
    photo backdrops (foliage, fabric, concrete)."""
    w, h = img.width, img.height
    base_cells = random.randint(3, 8)
    octaves = random.randint(2, 4)
    base = np.array(
        [random.randint(70, 200) for _ in range(3)], dtype=np.float32
    )
    variance = random.uniform(50.0, 120.0)
    layers = [
        _value_noise(w, h, base_cells=base_cells, octaves=octaves)
        for _ in range(3)
    ]
    arr = np.stack(
        [base[c] + (layers[c] - 0.5) * variance for c in range(3)], axis=-1
    )
    arr = np.clip(arr, 0, 255).astype(np.uint8)
    _paste_array(img, arr)


def apply_clutter(img: Image.Image) -> None:
    """Overlay rectangular colour patches and concentric-square decoys to
    stress finder-pattern discrimination. Decoys mimic the 7:5:3 module
    ratio of real QR finder patterns so a naive detector will latch onto
    them. Mutates img in place."""
    draw = ImageDraw.Draw(img)
    w, h = img.width, img.height

    n_rects = random.randint(8, 20)
    for _ in range(n_rects):
        rw = random.randint(20, max(21, w // 5))
        rh = random.randint(20, max(21, h // 5))
        x = random.randint(0, max(0, w - rw))
        y = random.randint(0, max(0, h - rh))
        color = tuple(random.randint(0, 255) for _ in range(3))
        draw.rectangle([x, y, x + rw, y + rh], fill=color)

    n_decoys = random.randint(2, 5)
    for _ in range(n_decoys):
        s = random.randint(15, 45)
        cx = random.randint(s, max(s + 1, w - s))
        cy = random.randint(s, max(s + 1, h - s))
        s2 = max(1, int(round(s * 5 / 7)))
        s3 = max(1, int(round(s * 3 / 7)))
        draw.rectangle([cx - s, cy - s, cx + s, cy + s], fill=(0, 0, 0))
        draw.rectangle(
            [cx - s2, cy - s2, cx + s2, cy + s2], fill=(255, 255, 255)
        )
        draw.rectangle([cx - s3, cy - s3, cx + s3, cy + s3], fill=(0, 0, 0))


def bg_radial(img: Image.Image) -> None:
    cx, cy = img.width / 2, img.height / 2
    max_dist = math.hypot(cx, cy)
    c1 = np.array([random.randint(200, 255) for _ in range(3)], dtype=np.float32)
    c2 = np.array([random.randint(128, 200) for _ in range(3)], dtype=np.float32)
    xs = np.arange(img.width, dtype=np.float32) - cx
    ys = np.arange(img.height, dtype=np.float32) - cy
    dist = np.clip(np.hypot(xs[None, :], ys[:, None]) / max_dist, 0.0, 1.0)
    arr = (c1 + (c2 - c1) * dist[..., None]).astype(np.uint8)
    _paste_array(img, arr)


BACKGROUNDS = [
    bg_solid,
    bg_gradient_horizontal,
    bg_gradient_vertical,
    bg_checkerboard,
    bg_stripes,
    bg_noise,
    bg_radial,
]


def make_qr(text: str) -> Image.Image:
    qr = qrcode.QRCode(
        error_correction=qrcode.constants.ERROR_CORRECT_L,
        box_size=10,
        border=0,
    )
    qr.add_data(text)
    qr.make(fit=True)
    img = qr.make_image(fill_color="black", back_color="white").convert("RGBA")
    pixels = img.load()
    for y in range(img.height):
        for x in range(img.width):
            if pixels[x, y][:3] == (255, 255, 255):
                pixels[x, y] = (0, 0, 0, 0)
    return img


def place_qr(
    bg: Image.Image, qr_img: Image.Image
) -> tuple[Image.Image, list[tuple[int, int]]]:
    """Place QR on background. Returns (image, corners) where corners are
    the 4 boundary points of the QR code in clockwise order starting top-left."""
    angle = random.uniform(0, 360)
    rad = math.radians(angle % 90)
    expand_factor = math.cos(rad) + math.sin(rad)

    target = min(bg.width, bg.height) * random.uniform(0.3, 0.8)
    qr_side = int(target / expand_factor)
    qr_side = max(qr_side, 1)

    qr_scaled = qr_img.resize((qr_side, qr_side), Image.NEAREST)
    qr_rotated = qr_scaled.rotate(angle, resample=Image.BICUBIC, expand=True)

    max_x = bg.width - qr_rotated.width
    max_y = bg.height - qr_rotated.height
    x = random.randint(0, max(max_x, 0))
    y = random.randint(0, max(max_y, 0))

    result = bg.copy().convert("RGBA")
    result.paste(qr_rotated, (x, y), mask=qr_rotated)

    s = qr_side
    cx_local = s / 2.0
    cy_local = s / 2.0
    cx_img = x + qr_rotated.width / 2.0
    cy_img = y + qr_rotated.height / 2.0
    theta = math.radians(angle)
    cos_a = math.cos(theta)
    sin_a = math.sin(theta)
    raw_corners = [
        (-cx_local, -cy_local),
        (cx_local, -cy_local),
        (cx_local, cy_local),
        (-cx_local, cy_local),
    ]
    corners = []
    for dx, dy in raw_corners:
        rx = dx * cos_a - dy * sin_a + cx_img
        ry = dx * sin_a + dy * cos_a + cy_img
        corners.append((round(rx), round(ry)))

    return result.convert("RGB"), corners


def generate(n: int, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    for i in range(n):
        text = random_text()
        w, h = random.choice(SIZES)
        bg_fn = random.choice(BACKGROUNDS)

        bg = Image.new("RGB", (w, h))
        bg_fn(bg)

        qr_img = make_qr(text)
        result, corners = place_qr(bg, qr_img)

        coords = "_".join(f"{x}-{y}" for x, y in corners)
        fname = f"{text}_{coords}.png"
        result.save(output_dir / fname)
        print(f"[{i+1}/{n}] {w}x{h} {bg_fn.__name__:25s} -> {fname}")


DIFFICULTY_DIR = Path("tests/qr_codes_difficulty")

ALNUM = string.ascii_letters + string.digits


def random_alnum_text(min_len: int = 3, max_len: int = 12) -> str:
    length = random.randint(min_len, max_len)
    return "".join(random.choice(ALNUM) for _ in range(length))


def make_qr_rgba(
    text: str,
    box_size: int = 10,
    border: int = 4,
    white_bg: bool = True,
    dark_color: tuple[int, int, int] = (0, 0, 0),
    light_color: tuple[int, int, int] = (255, 255, 255),
) -> Image.Image:
    """QR as RGBA. dark_color/light_color set the module palette (defaults
    black-on-white). If white_bg is True the light-coloured quiet zone is
    opaque and rotates with the modules; if False the light pixels are
    stripped to transparent so the photo background shows through between
    the dark modules (no rectangular frame)."""
    qr = qrcode.QRCode(
        error_correction=qrcode.constants.ERROR_CORRECT_M,
        box_size=box_size,
        border=border,
    )
    qr.add_data(text)
    qr.make(fit=True)
    img = qr.make_image(
        fill_color=dark_color, back_color=light_color
    ).convert("RGBA")
    if not white_bg:
        arr = np.asarray(img).copy()
        mask = np.all(
            arr[..., :3] == np.array(light_color, dtype=np.uint8), axis=-1
        )
        arr[mask] = (0, 0, 0, 0)
        img = Image.fromarray(arr, "RGBA")
    return img


def add_gaussian_noise(img: Image.Image, sigma: float) -> Image.Image:
    """Add gaussian noise to an RGB image (one sample per pixel, applied to
    all three channels like the original scalar implementation)."""
    arr = np.asarray(img, dtype=np.int16)
    noise = _NP_RNG.normal(0.0, sigma, size=arr.shape[:2]).astype(np.int16)
    arr = np.clip(arr + noise[..., None], 0, 255).astype(np.uint8)
    return Image.fromarray(arr, "RGB")


def reduce_contrast(img: Image.Image, factor: float) -> Image.Image:
    """Compress intensity range toward gray. factor in (0, 1]."""
    arr = np.asarray(img, dtype=np.float32)
    arr = 128.0 + (arr - 128.0) * factor
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")


def perspective_warp(
    img: Image.Image, magnitude: float
) -> Image.Image:
    """Apply perspective warp. magnitude is max corner offset fraction (0..0.3)."""
    w, h = img.size
    m = magnitude
    dst = [
        (random.uniform(0, m) * w, random.uniform(0, m) * h),
        (w - random.uniform(0, m) * w, random.uniform(0, m) * h),
        (w - random.uniform(0, m) * w, h - random.uniform(0, m) * h),
        (random.uniform(0, m) * w, h - random.uniform(0, m) * h),
    ]
    src = [(0, 0), (w, 0), (w, h), (0, h)]
    coeffs = _find_perspective_coeffs(dst, src)
    return img.transform(
        (w, h),
        Image.PERSPECTIVE,
        coeffs,
        resample=Image.BILINEAR,
        fillcolor=(255, 255, 255),
    )


def _find_perspective_coeffs(
    src: list[tuple[float, float]], dst: list[tuple[float, float]]
) -> list[float]:
    """Compute 8 coefficients for PIL's Image.PERSPECTIVE transform."""
    a = []
    b = []
    for (x, y), (X, Y) in zip(src, dst):
        a.append([X, Y, 1, 0, 0, 0, -x * X, -x * Y])
        a.append([0, 0, 0, X, Y, 1, -y * X, -y * Y])
        b.append(x)
        b.append(y)
    return _solve_8x8(a, b)


def _solve_8x8(a: list[list[float]], b: list[float]) -> list[float]:
    """Tiny Gaussian elimination solver for 8x8 systems."""
    n = 8
    m = [row[:] + [bv] for row, bv in zip(a, b)]
    for i in range(n):
        pivot = i
        for k in range(i + 1, n):
            if abs(m[k][i]) > abs(m[pivot][i]):
                pivot = k
        m[i], m[pivot] = m[pivot], m[i]
        for k in range(i + 1, n):
            factor = m[k][i] / m[i][i]
            for j in range(i, n + 1):
                m[k][j] -= factor * m[i][j]
    x = [0.0] * n
    for i in range(n - 1, -1, -1):
        s = m[i][n]
        for j in range(i + 1, n):
            s -= m[i][j] * x[j]
        x[i] = s / m[i][i]
    return x


def _place_simple(
    bg: Image.Image,
    qr: Image.Image,
    scale: float,
    angle: float,
) -> Image.Image:
    target = int(min(bg.width, bg.height) * scale)
    qr_scaled = qr.resize((target, target), Image.NEAREST)
    if abs(angle) > 0.01:
        # Transparent fill so the rotated QR's bounding-box corners show the
        # photo background, not a white axis-aligned halo — the QR's own
        # quiet zone (if any) rotates with the modules.
        qr_scaled = qr_scaled.rotate(
            angle,
            resample=Image.BICUBIC,
            expand=True,
            fillcolor=(255, 255, 255, 0),
        )
    max_x = max(bg.width - qr_scaled.width, 0)
    max_y = max(bg.height - qr_scaled.height, 0)
    x = random.randint(0, max_x) if max_x > 0 else 0
    y = random.randint(0, max_y) if max_y > 0 else 0
    out = bg.copy().convert("RGBA")
    out.paste(qr_scaled, (x, y), mask=qr_scaled)
    return out.convert("RGB")


def apply_illumination(img: Image.Image) -> Image.Image:
    """Composite one of {vignette, spotlight, linear ramp, specular} onto img.

    Brightness delta spans 40-70% across the image. Vignette/spotlight/linear
    are multiplicative falloffs that fool global Otsu; specular is additive
    highlight that locally saturates the background.
    """
    arr = np.asarray(img, dtype=np.float32) / 255.0
    h, w = arr.shape[:2]
    effect = random.choice(["vignette", "spotlight", "linear", "specular"])
    strength = random.uniform(0.55, 0.85)
    xs = np.arange(w, dtype=np.float32)
    ys = np.arange(h, dtype=np.float32)
    if effect == "vignette":
        cx, cy = w / 2.0, h / 2.0
        max_dist = math.hypot(cx, cy)
        dist = np.hypot(xs[None, :] - cx, ys[:, None] - cy) / max_dist
        mask = 1.0 - strength * np.clip(dist, 0.0, 1.0) ** 2
        arr = arr * mask[..., None]
    elif effect == "spotlight":
        cx = random.uniform(0.2, 0.8) * w
        cy = random.uniform(0.2, 0.8) * h
        radius = min(w, h) * random.uniform(0.25, 0.45)
        dist = np.hypot(xs[None, :] - cx, ys[:, None] - cy) / radius
        mask = 1.0 - strength * np.clip(dist, 0.0, 1.0) ** 2
        arr = arr * mask[..., None]
    elif effect == "linear":
        angle = random.uniform(0.0, 2.0 * math.pi)
        dx = math.cos(angle)
        dy = math.sin(angle)
        t = (xs[None, :] / max(w - 1, 1)) * dx + (ys[:, None] / max(h - 1, 1)) * dy
        span = float(t.max() - t.min()) or 1.0
        t_norm = (t - t.min()) / span
        mask = 1.0 - strength * t_norm
        arr = arr * mask[..., None]
    else:
        cx = random.uniform(0.15, 0.85) * w
        cy = random.uniform(0.15, 0.85) * h
        radius = min(w, h) * random.uniform(0.08, 0.2)
        dist = np.hypot(xs[None, :] - cx, ys[:, None] - cy) / radius
        highlight = strength * np.exp(-(dist ** 2))
        arr = arr + highlight[..., None]
    arr = np.clip(arr * 255.0, 0, 255).astype(np.uint8)
    return Image.fromarray(arr, "RGB")


def apply_jpeg(img: Image.Image, quality: int) -> Image.Image:
    """Round-trip through JPEG encode/decode to introduce 8x8 block ringing
    and chroma subsampling haze."""
    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=quality)
    buf.seek(0)
    return Image.open(buf).convert("RGB")


def apply_defocus_blur(img: Image.Image, radius: float) -> Image.Image:
    """Disk (bokeh) blur — uniform averaging within a circular kernel.
    Distinct from Gaussian: flat response inside the disk, hard falloff at
    the edge. Models an out-of-focus camera lens."""
    arr = np.asarray(img, dtype=np.float32)
    h, w = arr.shape[:2]
    r = int(math.ceil(radius))
    ys, xs = np.ogrid[-r : r + 1, -r : r + 1]
    mask = (xs * xs + ys * ys) <= radius * radius
    offsets = [
        (int(y), int(x))
        for y in range(-r, r + 1)
        for x in range(-r, r + 1)
        if mask[y + r, x + r]
    ]
    pad = r
    padded = np.pad(arr, ((pad, pad), (pad, pad), (0, 0)), mode="edge")
    out = np.zeros_like(arr)
    for oy, ox in offsets:
        out += padded[pad - oy : pad - oy + h, pad - ox : pad - ox + w, :]
    out /= float(len(offsets))
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGB")


def apply_motion_blur(
    img: Image.Image, length: int, angle_deg: float
) -> Image.Image:
    """Directional blur via sum of edge-padded shifts along a line. Sparse
    kernel (only `length` nonzero cells), so numpy shifts are fast enough."""
    arr = np.asarray(img, dtype=np.float32)
    h, w = arr.shape[:2]
    angle = math.radians(angle_deg)
    dx = math.cos(angle)
    dy = math.sin(angle)
    pad = length // 2 + 1
    padded = np.pad(
        arr, ((pad, pad), (pad, pad), (0, 0)), mode="edge"
    )
    out = np.zeros_like(arr)
    for i in range(length):
        t = i - (length - 1) / 2.0
        ox = int(round(t * dx))
        oy = int(round(t * dy))
        out += padded[pad - oy : pad - oy + h, pad - ox : pad - ox + w, :]
    out /= float(length)
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGB")


def apply_chromatic_aberration(
    img: Image.Image, max_shift: float
) -> Image.Image:
    """Shift R channel radially outward and B channel radially inward by up
    to max_shift pixels at the frame corners. Simulates the radial chromatic
    aberration of a cheap lens."""
    arr = np.asarray(img, dtype=np.float32)
    h, w = arr.shape[:2]
    cy, cx = h / 2.0, w / 2.0
    ys = np.arange(h, dtype=np.float32)[:, None]
    xs = np.arange(w, dtype=np.float32)[None, :]
    dx = xs - cx
    dy = ys - cy
    r = np.hypot(dx, dy)
    max_r = math.hypot(cx, cy) or 1.0
    frac = r / max_r
    safe_r = np.maximum(r, 1e-3)
    ux = dx / safe_r
    uy = dy / safe_r
    shift = max_shift * frac

    def sample(ch: np.ndarray, sx: np.ndarray, sy: np.ndarray) -> np.ndarray:
        x0 = np.clip(np.floor(sx).astype(np.int32), 0, w - 1)
        x1 = np.clip(x0 + 1, 0, w - 1)
        y0 = np.clip(np.floor(sy).astype(np.int32), 0, h - 1)
        y1 = np.clip(y0 + 1, 0, h - 1)
        fx = (sx - x0).astype(np.float32)
        fy = (sy - y0).astype(np.float32)
        a = ch[y0, x0] * (1 - fx) + ch[y0, x1] * fx
        b = ch[y1, x0] * (1 - fx) + ch[y1, x1] * fx
        return a * (1 - fy) + b * fy

    out = arr.copy()
    out[..., 0] = sample(arr[..., 0], xs - shift * ux, ys - shift * uy)
    out[..., 2] = sample(arr[..., 2], xs + shift * ux, ys + shift * uy)
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGB")


def apply_specular_glare(img: Image.Image, area_frac: float) -> Image.Image:
    """Additive elliptical highlight occupying roughly area_frac of the
    frame. Core saturates to pure white, halo falls off smoothly. Distinct
    from apply_illumination's specular (smaller, bigger lift, hard core)."""
    arr = np.asarray(img, dtype=np.float32)
    h, w = arr.shape[:2]
    aspect = random.uniform(0.4, 2.5)
    target_area = area_frac * w * h
    a = math.sqrt(target_area / math.pi * aspect)
    b = target_area / (math.pi * a) if a > 0 else 1.0
    a = max(a, 4.0)
    b = max(b, 4.0)
    cx = random.uniform(a, max(a + 1.0, w - a))
    cy = random.uniform(b, max(b + 1.0, h - b))
    ys = np.arange(h, dtype=np.float32)[:, None]
    xs = np.arange(w, dtype=np.float32)[None, :]
    r = np.hypot((xs - cx) / a, (ys - cy) / b)
    core = np.clip(1.0 - r, 0.0, 1.0)
    halo = np.exp(-r * r * 1.5)
    mask = core * 1.0 + halo * 0.35
    arr = arr + mask[..., None] * 320.0
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")


def apply_impulse_noise(img: Image.Image, prob: float) -> Image.Image:
    """Salt-and-pepper: each pixel flipped to pure black or white with the
    given total probability (half salt, half pepper). Harder than Gaussian
    noise for threshold-based detectors since the flips are extreme values."""
    arr = np.asarray(img, dtype=np.uint8).copy()
    h, w = arr.shape[:2]
    r = _NP_RNG.random((h, w), dtype=np.float32)
    salt = r < prob / 2.0
    pepper = (r >= prob / 2.0) & (r < prob)
    arr[salt] = (255, 255, 255)
    arr[pepper] = (0, 0, 0)
    return Image.fromarray(arr, "RGB")


def apply_row_shear(img: Image.Image, magnitude: float) -> Image.Image:
    """Horizontal shear: row y shifts by magnitude*(y - h/2) px. Models
    rolling-shutter skew from horizontal camera or subject motion. Unlike
    perspective this is non-planar — the rows stay horizontal but slide."""
    arr = np.asarray(img, dtype=np.float32)
    h, w = arr.shape[:2]
    ys = np.arange(h, dtype=np.float32)
    shifts = magnitude * (ys - h / 2.0)
    xs = np.arange(w, dtype=np.float32)
    src_x = xs[None, :] - shifts[:, None]
    x0 = np.clip(np.floor(src_x).astype(np.int32), 0, w - 1)
    x1 = np.clip(x0 + 1, 0, w - 1)
    fx = (src_x - x0).astype(np.float32)[..., None]
    row_idx = np.arange(h, dtype=np.int32)[:, None]
    out = arr[row_idx, x0, :] * (1.0 - fx) + arr[row_idx, x1, :] * fx
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGB")


def apply_physical_damage(img: Image.Image) -> Image.Image:
    """Simulate torn/creased printed paper: a couple of fold shadow bands
    (multiplicative darkening strips) plus small circular holes with thin
    dark borders."""
    out = np.asarray(img, dtype=np.float32).copy()
    h, w = out.shape[:2]
    n_folds = random.randint(1, 2)
    for _ in range(n_folds):
        band = random.randint(2, 6)
        intensity = random.uniform(0.35, 0.55)
        if random.random() < 0.5:
            y = random.randint(h // 5, 4 * h // 5)
            out[max(0, y - band) : min(h, y + band + 1), :, :] *= 1.0 - intensity
        else:
            x = random.randint(w // 5, 4 * w // 5)
            out[:, max(0, x - band) : min(w, x + band + 1), :] *= 1.0 - intensity
    ys = np.arange(h, dtype=np.float32)[:, None]
    xs = np.arange(w, dtype=np.float32)[None, :]
    n_holes = random.randint(2, 4)
    for _ in range(n_holes):
        r = random.randint(4, 12)
        cx = random.randint(r + 2, max(r + 3, w - r - 2))
        cy = random.randint(r + 2, max(r + 3, h - r - 2))
        dist = np.hypot(xs - cx, ys - cy)
        out[dist <= r] = (255.0, 255.0, 255.0)
        out[(dist > r) & (dist <= r + 1.5)] = (30.0, 30.0, 30.0)
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGB")


def apply_cylindrical_warp(
    img: Image.Image, angle_max: float, axis: str = "v"
) -> Image.Image:
    """Warp as if the image were wrapped around a cylinder viewed from
    outside. axis='v' wraps around a vertical cylinder (columns compress
    toward left/right edges), 'h' wraps around a horizontal cylinder (rows
    compress toward top/bottom). angle_max (radians) sets the visible arc;
    0 = flat, ~π/2 = hemicircle."""
    arr = np.asarray(img, dtype=np.float32)
    h, w = arr.shape[:2]
    if angle_max < 1e-4:
        return img.copy()
    sin_max = math.sin(angle_max)
    if axis == "v":
        t = np.linspace(-1.0, 1.0, w, dtype=np.float32)
        src = (np.sin(t * angle_max) / sin_max + 1.0) * 0.5 * (w - 1)
        x0 = np.clip(np.floor(src).astype(np.int32), 0, w - 1)
        x1 = np.clip(x0 + 1, 0, w - 1)
        fx = (src - x0).astype(np.float32)[None, :, None]
        out = arr[:, x0, :] * (1.0 - fx) + arr[:, x1, :] * fx
    else:
        t = np.linspace(-1.0, 1.0, h, dtype=np.float32)
        src = (np.sin(t * angle_max) / sin_max + 1.0) * 0.5 * (h - 1)
        y0 = np.clip(np.floor(src).astype(np.int32), 0, h - 1)
        y1 = np.clip(y0 + 1, 0, h - 1)
        fy = (src - y0).astype(np.float32)[:, None, None]
        out = arr[y0, :, :] * (1.0 - fy) + arr[y1, :, :] * fy
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGB")


def apply_qr_occlusion(
    qr_rgba: Image.Image, border: int, box_size: int
) -> Image.Image:
    """Paint opaque patches on the QR data area, skipping the three finder
    corners (with a one-module inner quiet-zone margin). Patches are applied
    to the QR image in its local coordinate system BEFORE rotation/placement
    so finder positions are exact regardless of downstream transforms."""
    total_modules = qr_rgba.size[0] // box_size
    modules = total_modules - 2 * border
    if modules < 21:
        return qr_rgba
    inner_lo = border * box_size
    inner_hi = (border + modules) * box_size
    finder_end = (border + 8) * box_size
    finder_start_br_x = (border + modules - 8) * box_size
    finder_start_br_y = finder_start_br_x
    exclusions = [
        (inner_lo, inner_lo, finder_end, finder_end),
        (finder_start_br_x, inner_lo, inner_hi, finder_end),
        (inner_lo, finder_start_br_y, finder_end, inner_hi),
    ]

    data_area = float((modules * box_size) ** 2)
    target_area = random.uniform(0.02, 0.10) * data_area

    out = qr_rgba.copy()
    draw = ImageDraw.Draw(out)
    used_area = 0.0
    attempts = 0
    placed = 0
    while used_area < target_area and attempts < 40:
        attempts += 1
        shape = random.choice(["rect", "ellipse"])
        pw = random.randint(box_size * 2, box_size * 6)
        ph = random.randint(box_size * 2, box_size * 6)
        cx = random.randint(inner_lo, inner_hi)
        cy = random.randint(inner_lo, inner_hi)
        x0, y0 = cx - pw // 2, cy - ph // 2
        x1, y1 = x0 + pw, y0 + ph
        hits_finder = False
        for ex in exclusions:
            if x1 > ex[0] and x0 < ex[2] and y1 > ex[1] and y0 < ex[3]:
                hits_finder = True
                break
        if hits_finder:
            continue
        color = (
            random.randint(0, 255),
            random.randint(0, 255),
            random.randint(0, 255),
            255,
        )
        box = [x0, y0, x1, y1]
        if shape == "rect":
            draw.rectangle(box, fill=color)
        else:
            draw.ellipse(box, fill=color)
        used_area += pw * ph
        placed += 1
    if placed == 0:
        # Guarantee at least one patch so the level isn't a silent no-op
        # when random attempts keep clipping finders.
        pw = box_size * 3
        ph = box_size * 3
        cx = (inner_lo + inner_hi) // 2
        cy = (inner_lo + inner_hi) // 2
        draw.rectangle(
            [cx - pw // 2, cy - ph // 2, cx + pw // 2, cy + ph // 2],
            fill=(0, 0, 0, 255),
        )
    return out


def _render_level(level: int, text: str) -> Image.Image:
    """Render one QR image at the given difficulty level (1-20)."""
    # Randomly decide whether the QR gets an opaque white quiet zone
    # (that rotates with the modules) or a transparent quiet zone (so the
    # photo background shows directly between the dark modules).
    white_bg = random.random() < 0.5

    if level == 1:
        # Trivial: large QR, white background, axis aligned.
        w, h = random.choice([(400, 400), (512, 512), (640, 480)])
        bg = Image.new("RGB", (w, h), (255, 255, 255))
        qr = make_qr_rgba(text, box_size=10, white_bg=white_bg)
        return _place_simple(bg, qr, scale=random.uniform(0.55, 0.8), angle=0.0)

    if level == 2:
        # Easy: solid color bg, small rotation.
        w, h = random.choice([(480, 480), (640, 480), (800, 600)])
        bg = Image.new("RGB", (w, h))
        bg_solid(bg)
        qr = make_qr_rgba(text, box_size=10, white_bg=white_bg)
        return _place_simple(
            bg,
            qr,
            scale=random.uniform(0.4, 0.65),
            angle=random.uniform(-15, 15),
        )

    if level == 3:
        # Moderate: varied backgrounds, full rotation.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        return _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )

    if level == 4:
        # Hard: blur + noise + smaller QR.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.25, 0.4),
            angle=random.uniform(0, 360),
        )
        img = img.filter(ImageFilter.GaussianBlur(radius=random.uniform(0.8, 1.6)))
        img = add_gaussian_noise(img, sigma=random.uniform(6.0, 12.0))
        return img

    if level == 5:
        # Very hard: perspective warp, low contrast, blur, small size.
        w, h = random.choice([(640, 480), (800, 600), (1024, 768)])
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, border=4, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.25, 0.35),
            angle=random.uniform(0, 360),
        )
        img = perspective_warp(img, magnitude=random.uniform(0.06, 0.14))
        img = img.filter(
            ImageFilter.GaussianBlur(radius=random.uniform(1.0, 1.8))
        )
        img = reduce_contrast(img, factor=random.uniform(0.55, 0.75))
        img = add_gaussian_noise(img, sigma=random.uniform(4.0, 9.0))
        return img

    if level == 6:
        # Uneven illumination on a level-3-style render.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )
        return apply_illumination(img)

    if level == 7:
        # JPEG compression artefacts over a level-3-style render.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )
        quality = random.randint(5, 20)
        return apply_jpeg(img, quality)

    if level == 8:
        # Directional motion blur over a level-3-style render.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )
        length = random.randint(5, 12)
        blur_angle = random.uniform(0.0, 360.0)
        return apply_motion_blur(img, length, blur_angle)

    if level == 9:
        # Occlusion patches on the QR data area, finders preserved.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        box_size = 8
        border = 4
        qr = make_qr_rgba(
            text, box_size=box_size, border=border, white_bg=white_bg
        )
        qr = apply_qr_occlusion(qr, border=border, box_size=box_size)
        return _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )

    if level == 10:
        # Photo-like value-noise background plus rectangular clutter and
        # concentric-square decoys that mimic finder patterns.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        bg_photo_like(bg)
        apply_clutter(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        return _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )

    if level == 11:
        # Defocus (disk) blur over a level-3-style render. Kernel shape
        # distinct from Gaussian (level_4) and directional (level_8).
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )
        return apply_defocus_blur(img, radius=random.uniform(3.0, 7.0))

    if level == 12:
        # Cylindrical warp. Non-planar geometry — the image is wrapped
        # around a virtual cylinder so edges are compressed relative to
        # center. Distinct from level_5's planar perspective transform.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )
        angle_max = random.uniform(0.5, 1.0)
        axis = random.choice(["v", "h"])
        return apply_cylindrical_warp(img, angle_max=angle_max, axis=axis)

    if level == 13:
        # Inverted or coloured QR modules. Spec-legal palettes that detectors
        # often miss: white-on-black, navy-on-cream, etc. Stresses
        # binarisation polarity assumptions.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        mode = random.choice(["inverted", "colored", "colored_inverted"])
        if mode == "inverted":
            dark = (255, 255, 255)
            light = (0, 0, 0)
        elif mode == "colored":
            dark = tuple(random.randint(0, 90) for _ in range(3))
            light = tuple(random.randint(190, 255) for _ in range(3))
        else:
            dark = tuple(random.randint(190, 255) for _ in range(3))
            light = tuple(random.randint(0, 90) for _ in range(3))
        qr = make_qr_rgba(
            text,
            box_size=8,
            white_bg=white_bg,
            dark_color=dark,
            light_color=light,
        )
        return _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )

    if level == 14:
        # Combined real-world. Small QR + perspective warp + random subset
        # of {blur, noise, JPEG, uneven lighting} applied in capture-chain
        # order. Models a phone photograph of a printed QR: several moderate
        # degradations compounding, rather than one aggressive stressor.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.22, 0.35),
            angle=random.uniform(0, 360),
        )
        img = perspective_warp(img, magnitude=random.uniform(0.04, 0.09))
        blur_kind = random.choice([None, "gauss", "defocus", "motion"])
        others = random.sample(
            ["noise", "jpeg", "illum"], k=random.randint(1, 2)
        )
        if "illum" in others:
            img = apply_illumination(img)
        if blur_kind == "defocus":
            img = apply_defocus_blur(img, radius=random.uniform(1.5, 3.5))
        elif blur_kind == "motion":
            img = apply_motion_blur(
                img,
                length=random.randint(3, 7),
                angle_deg=random.uniform(0, 360),
            )
        elif blur_kind == "gauss":
            img = img.filter(
                ImageFilter.GaussianBlur(radius=random.uniform(0.4, 1.0))
            )
        if "noise" in others:
            img = add_gaussian_noise(img, sigma=random.uniform(3.0, 7.0))
        if "jpeg" in others:
            img = apply_jpeg(img, quality=random.randint(30, 55))
        return img

    if level == 15:
        # Chromatic aberration: radial R/B channel offset simulating cheap
        # lens dispersion. Stresses grayscale-conversion assumptions.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )
        return apply_chromatic_aberration(img, max_shift=random.uniform(2.0, 5.0))

    if level == 16:
        # Specular glare / reflection: large additive highlight that
        # saturates a portion of the QR. Distinct from level_6 illumination:
        # modules in the hot region are fully clipped, not just dimmed.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )
        return apply_specular_glare(img, area_frac=random.uniform(0.05, 0.18))

    if level == 17:
        # Impulse (salt-and-pepper) noise: fraction of pixels flipped to
        # pure black or white. Harder than Gaussian for threshold-based
        # detectors because flips are out-of-distribution.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )
        return apply_impulse_noise(img, prob=random.uniform(0.005, 0.03))

    if level == 18:
        # Tiny / distant QR: occupies 8-15% of the frame. Finder-ring
        # detection stressed at low pixel counts — a finder center only
        # spans a handful of pixels.
        w, h = random.choice([(800, 600), (1024, 768), (1280, 960), (1920, 1080)])
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        return _place_simple(
            bg,
            qr,
            scale=random.uniform(0.08, 0.15),
            angle=random.uniform(0, 360),
        )

    if level == 19:
        # Rolling-shutter shear: horizontal shift proportional to row index.
        # Non-planar — distinct from level_5's global perspective.
        w, h = random.choice(SIZES)
        bg = Image.new("RGB", (w, h))
        random.choice(BACKGROUNDS)(bg)
        qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
        img = _place_simple(
            bg,
            qr,
            scale=random.uniform(0.35, 0.55),
            angle=random.uniform(0, 360),
        )
        magnitude = random.uniform(0.05, 0.18) * random.choice([-1.0, 1.0])
        return apply_row_shear(img, magnitude=magnitude)

    # Level 20: physical damage. Paper fold shadows + punched holes on a
    # level-3-style render. Distinct from level_9 opaque patches: folds
    # multiplicatively darken strips and holes leave white voids with
    # thin dark rims, not random-coloured shapes.
    w, h = random.choice(SIZES)
    bg = Image.new("RGB", (w, h))
    random.choice(BACKGROUNDS)(bg)
    qr = make_qr_rgba(text, box_size=8, white_bg=white_bg)
    img = _place_simple(
        bg,
        qr,
        scale=random.uniform(0.35, 0.55),
        angle=random.uniform(0, 360),
    )
    return apply_physical_damage(img)


def _zxing_decodes(path: Path, expected: str) -> bool:
    """Return True iff zxing-cpp decodes `expected` from the image."""
    import zxingcpp

    try:
        img = Image.open(path)
        results = zxingcpp.read_barcodes(img)
    except Exception:
        return False
    for r in results:
        if r.text == expected:
            return True
    return False


def _clear_level_dir(output_dir: Path, level: int) -> Path:
    level_dir = output_dir / f"level_{level}"
    level_dir.mkdir(parents=True, exist_ok=True)
    for old in level_dir.glob("*.png"):
        old.unlink()
    return level_dir


def _validate_difficulty(
    seed: int,
    per_level: int,
    levels: list[int],
    output_dir: Path,
    max_attempts: int = 8,
) -> dict:
    """Generate + zxing-cpp validate + record the accept/reject pattern.

    The returned dict is the manifest: every text tried (accepted or dropped)
    is logged along with the number of render attempts it consumed and which
    attempt (if any) the validator accepted. Replaying this manifest reseeds
    the RNG with the recorded seed and consumes RNG in the same order, so
    the validator-free regeneration produces byte-identical images.
    """
    manifest: dict = {
        "seed": seed,
        "per_level": per_level,
        "max_attempts": max_attempts,
        "levels": {},
    }

    for level in levels:
        level_dir = _clear_level_dir(output_dir, level)
        entries: list[dict] = []
        accepted = 0
        safety = 0
        while accepted < per_level:
            safety += 1
            if safety > per_level * max_attempts * 3:
                break
            text = random_alnum_text()
            accepted_at = -1
            attempts = 0
            for attempt in range(max_attempts):
                attempts += 1
                try:
                    img = _render_level(level, text)
                except Exception:
                    continue
                fname = f"{accepted:04d}_{text}.png"
                fpath = level_dir / fname
                img.save(fpath)
                if _zxing_decodes(fpath, text):
                    accepted_at = attempt
                    accepted += 1
                    break
                fpath.unlink(missing_ok=True)
            # Default case (1 render, accepted on attempt 0) is encoded as
            # a bare string so the manifest doesn't carry 145 copies of the
            # same `"attempts": 1, "accepted_at": 0` boilerplate. Retries
            # and drops use an object with the explicit counts.
            entry: str | dict
            if attempts == 1 and accepted_at == 0:
                entry = text
            else:
                entry = {
                    "text": text,
                    "attempts": attempts,
                    "accepted_at": accepted_at,
                }
            entries.append(entry)

        manifest["levels"][str(level)] = entries
        kept = sum(
            1
            for e in entries
            if isinstance(e, str) or e.get("accepted_at", 0) >= 0
        )
        dropped = len(entries) - kept
        print(
            f"Level {level}: {kept} accepted, {dropped} texts dropped "
            f"(zxing-cpp-validated) -> {level_dir}"
        )

    return manifest


def _replay_difficulty(
    levels: list[int],
    output_dir: Path,
    manifest: dict,
) -> None:
    """Replay a validated manifest: consume RNG in the recorded order and
    save only the attempts the validator originally accepted. No decoder
    needed on the replay path."""
    level_entries = manifest["levels"]
    for level in levels:
        entries = level_entries.get(str(level))
        if entries is None:
            raise RuntimeError(
                f"Manifest has no entry for level_{level}; re-run with --validate"
            )
        level_dir = _clear_level_dir(output_dir, level)
        accepted = 0
        for entry in entries:
            if isinstance(entry, str):
                expected_text = entry
                attempts = 1
                accepted_at = 0
            else:
                expected_text = entry["text"]
                attempts = entry.get("attempts", 1)
                accepted_at = entry.get("accepted_at", 0)
            text = random_alnum_text()
            if text != expected_text:
                raise RuntimeError(
                    f"Manifest desync at level_{level} index {accepted}: "
                    f"expected text {expected_text!r}, got {text!r}. "
                    f"Re-run with --validate to refresh the manifest."
                )
            for attempt in range(attempts):
                img = _render_level(level, text)
                if attempt == accepted_at:
                    fname = f"{accepted:04d}_{text}.png"
                    img.save(level_dir / fname)
            if accepted_at >= 0:
                accepted += 1
        print(
            f"Level {level}: {accepted} regenerated (manifest-replay) -> {level_dir}"
        )


def generate_difficulty(
    seed: int,
    per_level: int,
    levels: list[int],
    output_dir: Path,
    validate: bool = False,
    max_attempts: int = 8,
) -> None:
    """Dispatch: validate with zxing-cpp (and write manifest) or replay."""
    if validate:
        _seed_rngs(seed)
        manifest = _validate_difficulty(
            seed, per_level, levels, output_dir, max_attempts
        )
        MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
        with MANIFEST_PATH.open("w") as f:
            json.dump(manifest, f, indent=2)
            f.write("\n")
        print(f"\nWrote validation manifest to {MANIFEST_PATH}")
        return
    if not MANIFEST_PATH.exists():
        raise RuntimeError(
            f"No validation manifest at {MANIFEST_PATH}. "
            f"Run once with --validate to create it."
        )
    with MANIFEST_PATH.open() as f:
        manifest = json.load(f)
    # Reseed from the manifest so replay always matches the validated run,
    # regardless of what --seed was passed on the command line.
    _seed_rngs(manifest["seed"])
    _replay_difficulty(levels, output_dir, manifest)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--difficulty",
        action="store_true",
        help="Generate tiered difficulty dataset (default: replay manifest).",
    )
    p.add_argument(
        "--validate",
        action="store_true",
        help="Re-run zxing-cpp validation and rewrite the manifest. "
        "Without this flag the generator replays the stored manifest.",
    )
    p.add_argument(
        "--per-level",
        type=int,
        default=30,
        help="Images per difficulty level (default: 30).",
    )
    p.add_argument(
        "--levels",
        type=str,
        default="1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20",
        help="Comma-separated list of levels to generate "
        "(default: 1..20).",
    )
    p.add_argument("--seed", type=int, default=42)
    p.add_argument(
        "--count",
        type=int,
        default=20,
        help="Count for random mode (ignored in --difficulty).",
    )
    return p.parse_args()


if __name__ == "__main__":
    args = parse_args()

    if args.difficulty:
        # generate_difficulty owns RNG seeding: --validate uses args.seed,
        # replay uses the seed stored in the manifest.
        levels = [int(x) for x in args.levels.split(",") if x.strip()]
        generate_difficulty(
            args.seed,
            args.per_level,
            levels,
            DIFFICULTY_DIR,
            validate=args.validate,
        )
        print(f"\nDone. Difficulty dataset in {DIFFICULTY_DIR}/")
    else:
        _seed_rngs(args.seed)
        generate(args.count, OUTPUT_DIR)
        print(f"\nDone. {args.count} images in {OUTPUT_DIR}/")
