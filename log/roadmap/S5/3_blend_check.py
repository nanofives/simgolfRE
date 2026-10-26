"""S5.3: check the 5-5-5 ClearType blend model against the GDI table 3_blend555.npy ([level 0..6, text 5-bit, surface
5-bit] -> stored 5-bit, measured by the first worker with TextOutA on a 16-bit DIB, white levels from a 32-bit draw).

Model: dec / enc = 8-bit gamma tables, round half up, gamma = contrast / 1000; F = dec[f << 3], D = dec[b << 3];
m = D + floor(((F - D) * al + den / 2) / den) with al = round(level * den / 6); out = enc[m] >> 3."""
import pathlib

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
T5 = np.load(HERE / "3_blend555.npy")
f5 = np.arange(32)[:, None]
b5 = np.arange(32)[None, :]
i = np.arange(256)
for contrast in (1200,):
    g = contrast / 1000
    dec = np.floor(255 * (i / 255) ** g + 0.5).astype(np.int64)
    enc = np.floor(255 * (i / 255) ** (1 / g) + 0.5).astype(np.int64)
    F = np.broadcast_to(dec[f5 << 3], (32, 32))
    D = np.broadcast_to(dec[b5 << 3], (32, 32))
    for den in (6, 1024, 4096, 65536):
        bad = 0
        for a in range(7):
            al = (a * den * 2 + 6) // 12
            m = D + np.floor_divide((F - D) * al + den // 2, den)
            bad += int(((enc[m] >> 3) != T5[a]).sum())
        print(f"contrast {contrast} den {den}: mismatches {bad} of {7 * 1024}")
