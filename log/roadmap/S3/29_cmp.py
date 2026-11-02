"""S3.29: GlRaster against the real GL's frames of the S3.29 / S3.21 probe scenes, per knob set.

    cd <tree> && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/29_cmp.py [--scenes a,b] [knobs ...]   knobs like "32=1"
"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import glprobe as P, lib as L  # noqa: E402

SCENES = ["s329_uramp_lin", "s329_uramp_near", "s329_vramp_lin", "s329_cramp_lin", "s329_cramp_near", "s329_cramp_untex",
          "s329_tris_lin", "s321_ramp_dense", "s321_gour_untex", "s321_gour_const", "s321_const_rand", "s321_gour_rand"]


def main():
    args = sys.argv[1:]
    scenes = SCENES
    if "--scenes" in args:
        i = args.index("--scenes"); scenes = args[i + 1].split(","); del args[i:i + 2]
    lib = L.load()
    for ks in args or [""]:
        knobs = {int(a): int(b) for a, b in (kv.split("=") for kv in ks.split(",") if kv)}
        out = []
        for sc in scenes:
            d = P.PROBE / sc
            if not (d / "snaps" / "s1" / "dib_post.bin").exists():
                continue
            real = np.frombuffer((d / "snaps" / "s1" / "dib_post.bin").read_bytes(), "<u2")
            port = P.port_frame(lib, d, knobs or None)
            res = P.report(real, port, True, sc)
            out.append(f"{sc[5:]}:{res[0]}/{res[1]}/c{res[2]}")
        print(f"{ks or 'default':10s} " + " ".join(out), flush=True)


if __name__ == "__main__":
    main()
