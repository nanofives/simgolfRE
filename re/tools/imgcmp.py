"""Tolerant screenshot comparison for screen-state checks (not pixel-exact parity).

Both images are reduced to 160x120 grayscale and compared by mean absolute difference (0..255).
Animated elements (cursor, highlights) stay well under the threshold; a different screen does not.
"""
from __future__ import annotations

import time

import numpy as np
from PIL import Image

SIZE = (160, 120)
DEFAULT_THRESHOLD = 12.0


def _norm(img: Image.Image) -> np.ndarray:
    return np.asarray(img.convert("L").resize(SIZE, Image.BILINEAR), dtype=np.float32)


def load(path) -> Image.Image:
    return Image.open(path).copy()


def distance(a: Image.Image, b: Image.Image) -> float:
    return float(np.abs(_norm(a) - _norm(b)).mean())


def matches(a: Image.Image, b: Image.Image, threshold: float = DEFAULT_THRESHOLD) -> bool:
    return distance(a, b) <= threshold


def wait_for(game, ref: Image.Image, timeout: float = 20, threshold: float = DEFAULT_THRESHOLD, poll: float = 0.5):
    """Poll the game window until it matches ref. Returns the matching screenshot; raises TimeoutError."""
    t0, best = time.time(), None
    while time.time() - t0 < timeout:
        if not game.alive:
            raise RuntimeError(f"game died while waiting for screen: {game.detach_reason}")
        shot = game.shot()
        d = distance(shot, ref)
        best = d if best is None else min(best, d)
        if d <= threshold:
            return shot
        time.sleep(poll)
    raise TimeoutError(f"screen never matched reference (best distance {best:.1f} > {threshold})")


def changed_fraction(a: Image.Image, b: Image.Image, delta: int = 60) -> float:
    """Fraction of pixels whose grayscale value moved by more than `delta` (full resolution).
    For small text (a date), the mean distance dilutes the change; this does not."""
    x = np.asarray(a.convert("L"), dtype=np.int16)
    y = np.asarray(b.convert("L"), dtype=np.int16)
    return float((np.abs(x - y) > delta).mean())


# ------------------------------------------------------------------------------------------------ pixel-exact (phase 5)
def rgb555_to_rgb(words: np.ndarray) -> np.ndarray:
    """RGB555 words (h x w) -> RGB bytes as scenario_ab.py's back-buffer frames expand them: (v << 3) | (v >> 2)."""
    w = words.astype(np.uint16)
    ch = np.stack([(w >> 10) & 31, (w >> 5) & 31, w & 31], axis=2).astype(np.uint8)
    return (ch << 3) | (ch >> 2)


def exact_diff(a, b, regions: dict | None = None) -> dict:
    """Pixel-exact comparison of two same-size frames (PIL images or h x w x 3 arrays). Returns {"total": n differing pixels,
    "regions": {name: n}, "bbox": (x0, y0, x1, y1) of the differing pixels or None}. A region is (x, y, w, h); the special
    region list value "rest" counts the pixels outside every other region."""
    x = np.asarray(a.convert("RGB") if hasattr(a, "convert") else a, dtype=np.uint8)
    y = np.asarray(b.convert("RGB") if hasattr(b, "convert") else b, dtype=np.uint8)
    if x.shape != y.shape:
        raise ValueError(f"frame sizes differ: {x.shape} vs {y.shape}")
    d = (x != y).any(axis=2)
    out = {"total": int(d.sum()), "regions": {}, "bbox": None}
    if out["total"]:
        ys, xs = np.nonzero(d)
        out["bbox"] = (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))
    if regions:
        covered = np.zeros(d.shape, dtype=bool)
        for name, (rx, ry, rw, rh) in regions.items():
            sl = (slice(max(ry, 0), max(ry + rh, 0)), slice(max(rx, 0), max(rx + rw, 0)))
            out["regions"][name] = int(d[sl].sum())
            covered[sl] = True
        out["regions"]["rest"] = int((d & ~covered).sum())
    return out
