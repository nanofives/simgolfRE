"""S4.11's front end over port/test/sound_replay.py, for the time before sound_replay.py calls sound_keys_crt.install itself
(11_merge.md section 3): installs step S4.11 (port/test/sound_keys_crt.py; its fixtures port/test/sound_fixtures/s411_crt.js are
loaded by the harness like every fixture file) and forwards to sound_replay's CLI.

    ~/bin/py -3.12 log/roadmap/S4/11_replay.py oracle --step S4.11 [--keys ...] [--update] [--out tsv] [--coverage tsv]
    ~/bin/py -3.12 log/roadmap/S4/11_replay.py replay --step S4.11 [--w32]
    ~/bin/py -3.12 log/roadmap/S4/11_replay.py negctl --step S4.11 [--mode 1]
(SG_PORTSIM_DLL = port/build/S4_11_mirror/x64/Release/portsim_c.dll, SG_S4_MIRROR = port/build/S4_11_mirror for --w32, built by
11_mirror.py; SG_EMU_ORACLE=1 for oracle / negctl.)
"""
from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import sound_keys_crt as K  # noqa: E402
from port.test import sound_replay as R  # noqa: E402

K.install(R)

if __name__ == "__main__":
    sys.exit(R.main(sys.argv[1:]))
