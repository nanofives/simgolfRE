"""S4.9: run S4.2's converter (re/tools/snd_hook2port.py, not edited) so that a function it cannot parse becomes a TODO block
instead of ending the whole run (collect() raises SystemExit, which main()'s per-row `except Exception` does not catch).

    ~/bin/py -3.12 log/roadmap/S4/9_convert.py --step S4.9 --module audio_wave --compile --out log/roadmap/S4/9_conv/audio_wave.cpp
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import snd_hook2port as C  # noqa: E402

_collect = C.collect


def collect(src, detour):
    try:
        return _collect(src, detour)
    except SystemExit as e:
        raise RuntimeError(str(e)) from None


C.collect = collect

if __name__ == "__main__":
    sys.exit(C.main())
