#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = []
# ///
"""Run flatcv's QR decoder against the tiered difficulty dataset
(tests/qr_codes_difficulty/level_*) and report pass rates per tier.

Usage:
    uv run tests/qr_difficulty_report.py [--flatcv ./flatcv]

The dataset must be produced first via:
    uv run tests/generate_qr_test_images.py --difficulty
"""

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

DIFFICULTY_DIR = Path("tests/qr_codes_difficulty")


def find_flatcv(explicit: str | None) -> Path:
    if explicit:
        p = Path(explicit)
        if not p.exists():
            sys.exit(f"flatcv binary not found at {p}")
        return p
    for candidate in ("flatcv", "flatcv_mac", "flatcv_linux"):
        p = Path(candidate)
        if p.exists():
            return p
    sys.exit("No flatcv binary found. Run `make flatcv` first.")


_QR_BLOCK_RE = re.compile(r'"qr_codes"\s*:\s*\[(.*?)\]\s*}', re.DOTALL)
_TEXT_RE = re.compile(r'"text"\s*:\s*"((?:[^"\\]|\\.)*)"')


def decode_with_flatcv(
    flatcv: Path, image: Path, tmp_out: Path
) -> list[str]:
    """Run flatcv and return the list of decoded texts."""
    try:
        proc = subprocess.run(
            [str(flatcv), str(image), "qr", str(tmp_out)],
            capture_output=True,
            text=True,
            timeout=30,
        )
    except subprocess.TimeoutExpired:
        return []
    if proc.returncode != 0:
        return []

    # The CLI prints diagnostic lines before a JSON-ish block. Extract
    # the qr_codes array and pull text fields from it.
    stdout = proc.stdout
    m = _QR_BLOCK_RE.search(stdout)
    if not m:
        return []
    block = m.group(1)
    texts = []
    for tm in _TEXT_RE.finditer(block):
        raw = tm.group(1)
        # Unescape minimally.
        raw = raw.encode("utf-8").decode("unicode_escape")
        texts.append(raw)
    return texts


def _expected_from_filename(fname: str) -> str | None:
    """Filename format: <index>_<text>.png. Return text, or None if malformed."""
    if not fname.endswith(".png"):
        return None
    stem = fname[:-4]
    idx = stem.find("_")
    if idx < 0 or idx == len(stem) - 1:
        return None
    return stem[idx + 1 :]


def evaluate_level(flatcv: Path, level_dir: Path) -> dict:
    images = sorted(level_dir.glob("*.png"))
    passed = 0
    failures = []
    with tempfile.TemporaryDirectory() as td:
        tmp_out = Path(td) / "out.png"
        for img in images:
            expected = _expected_from_filename(img.name)
            if expected is None:
                continue
            texts = decode_with_flatcv(flatcv, img, tmp_out)
            if expected in texts:
                passed += 1
            else:
                failures.append((img.name, expected, texts[0] if texts else None))
    return {
        "total": len([i for i in images if _expected_from_filename(i.name)]),
        "passed": passed,
        "failures": failures,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--flatcv", default=None)
    ap.add_argument(
        "--show-failures",
        action="store_true",
        help="Print per-image failure details.",
    )
    args = ap.parse_args()

    flatcv = find_flatcv(args.flatcv)
    if not DIFFICULTY_DIR.exists():
        sys.exit(
            f"{DIFFICULTY_DIR} not found. Generate it with: "
            "uv run tests/generate_qr_test_images.py --difficulty"
        )

    levels = sorted(
        [p for p in DIFFICULTY_DIR.iterdir() if p.is_dir() and p.name.startswith("level_")],
        key=lambda p: int(p.name.split("_")[1]),
    )
    if not levels:
        sys.exit(f"No level_* directories in {DIFFICULTY_DIR}")

    print(f"flatcv QR detection report (binary: {flatcv})")
    print(f"Dataset: {DIFFICULTY_DIR}")
    print()
    print(f"{'Level':<8}{'Passed':>10}{'Total':>10}{'Rate':>12}")
    print("-" * 40)

    grand_pass = 0
    grand_total = 0
    per_level_results = {}
    for ldir in levels:
        res = evaluate_level(flatcv, ldir)
        per_level_results[ldir.name] = res
        total = res["total"]
        passed = res["passed"]
        rate = (passed / total * 100) if total else 0.0
        grand_pass += passed
        grand_total += total
        print(
            f"{ldir.name:<8}{passed:>10}{total:>10}{rate:>11.1f}%"
        )

    print("-" * 40)
    grand_rate = (grand_pass / grand_total * 100) if grand_total else 0.0
    print(f"{'TOTAL':<8}{grand_pass:>10}{grand_total:>10}{grand_rate:>11.1f}%")

    if args.show_failures:
        print("\nFailures:")
        for level_name, res in per_level_results.items():
            if not res["failures"]:
                continue
            print(f"\n  {level_name}:")
            for fname, expected, got in res["failures"]:
                got_str = f'"{got}"' if got else "(nothing)"
                print(f"    {fname}: expected \"{expected}\", got {got_str}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
