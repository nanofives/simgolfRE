"""S4.2b development runner: while wave 1 (S4.1 + S4.2) is uncommitted, S4.2b's versions of the wave-1 files live in
log/roadmap/S4/2b_wave1/port/ (the tree's copies stay as accepted). This runner makes `port.test.sound_replay` resolve to
the copy, points the harness at the S4.2b mirror (port/build/S4_2b_mirror) and, unless set, at development corpora (bundles
port/corpus/audio_2b, goldens log/roadmap/S4/2b_goldens), then runs sound_replay's CLI or pytest:

    ~/bin/py -3.12 log/roadmap/S4/2b_dev.py sr oracle --step S4.6 --update --out ...
    ~/bin/py -3.12 log/roadmap/S4/2b_dev.py pytest port/test/test_audio_chan.py -q
Once wave 1 is committed and the copies are installed in the tree, plain `-m port.test.sound_replay` / `-m pytest` do the same
(with SG_SOUND_BUNDLES / SG_SOUND_GOLDENS unset they use port/corpus/audio and port/test).
"""
import importlib.util
import os
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
HERE = pathlib.Path(__file__).resolve().parent
COPY = HERE / "2b_wave1" / "port" / "test" / "sound_replay.py"
os.environ.setdefault("SG_SOUND_BUNDLES", str(ROOT / "port" / "corpus" / "audio_2b"))
os.environ.setdefault("SG_SOUND_GOLDENS", str(HERE / "2b_goldens"))
os.environ.setdefault("SG_S4_MIRROR", str(ROOT / "port" / "build" / "S4_2b_mirror"))
sys.path.insert(0, str(ROOT))
import port.test  # noqa: E402

spec = importlib.util.spec_from_file_location("port.test.sound_replay", COPY)
mod = importlib.util.module_from_spec(spec)
sys.modules["port.test.sound_replay"] = mod
port.test.sound_replay = mod
spec.loader.exec_module(mod)

if __name__ == "__main__":
    what, args = sys.argv[1], sys.argv[2:]
    if what == "sr":
        sys.exit(mod.main(args))
    if what == "pytest":
        import pytest
        sys.exit(pytest.main(args))
    raise SystemExit(f"unknown command {what}")
