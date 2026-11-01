"""S3.21b: GlRaster against the real GL's frames of the S3.21 probe scenes (written by 21_ramp.py dense and 21_modprobe.py; their
real frame is snaps/s1/dib_post.bin), with knob sets: default (stepping off), `#tune 30 1` (colour stepping), `30=1,31=F` (texture
coordinates stepped too).

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/21b_probes.py [knobs ...]   knobs like "30=1,31=16"
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glprobe as P, lib as L  # noqa: E402

SCENES = ["s321_ramp_dense", "s321_gour_untex", "s321_gour_const", "s321_const_rand", "s321_gour_rand"]


def main():
    lib = L.load()
    sets = sys.argv[1:] or ["", "30=1", "30=1,31=16"]
    for ks in sets:
        knobs = {int(a): int(b) for a, b in (kv.split("=") for kv in ks.split(",") if kv)}
        out = []
        for sc in SCENES:
            d = P.PROBE / sc
            if not (d / "snaps" / "s1" / "dib_post.bin").exists():
                continue
            import numpy as np
            real = np.frombuffer((d / "snaps" / "s1" / "dib_post.bin").read_bytes(), "<u2")
            port = P.port_frame(lib, d, knobs or None)
            res = P.report(real, port, True, sc)
            out.append(f"{sc}:{res[0]}/{res[1]}/cov{res[2]}")
        print(f"{ks or 'default':12s} " + "  ".join(out), flush=True)


if __name__ == "__main__":
    main()
