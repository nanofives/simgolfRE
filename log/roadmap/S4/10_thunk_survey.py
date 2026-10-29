"""S4.10, U-1415: sound.dll's incremental-link thunk table (the run of `jmp rel32` at the start of .text) against the port's
registered sound.dll functions (`snd.<hooks.csv name>`, `snd.FUN_<va>` for unnamed ones, and the CRT names audio_crt
registers). Every thunk target is classed: PORTED (a registered port function), or not ported with its hooks.csv status
(dead / trivial / held / level) so the gap is visible. Writes log/roadmap/S4/10_thunk_survey.tsv.

    ~/bin/py -3.12 log/roadmap/S4/10_thunk_survey.py [--dll <portsim_c.dll>]
"""
import argparse
import collections
import csv
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "port" / "test"))
import sound_emu as E  # noqa: E402
from port.test import audio_stream as AS  # noqa: E402
from port.test import lib as L  # noqa: E402


def thunk_table(data, secs):
    text = next(s for s in secs if s[3].startswith(".text"))
    lo = text[0]
    out, a = [], lo
    while a < len(data) and data[a] == 0xCC:         # the table starts after the section's leading padding
        a += 1
    while a + 5 <= len(data) and data[a] == 0xE9:
        t = (a + 5 + struct.unpack_from("<i", data, a + 1)[0]) & 0xFFFFFFFF
        out.append((a, t))
        a += 5
        while a < len(data) and data[a] == 0xCC:     # padding between thunks, if any
            a += 1
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll")
    a = ap.parse_args()
    lib = L.load(pathlib.Path(a.dll) if a.dll else AS.mirror_dll(), rebuild=False)
    fns = set(lib.functions())
    data, relocs, secs, exps, entry, ibase = E._raw(str(E.SOUND_DLL))
    rows = {}
    with open(ROOT / "hooks.csv", newline="") as f:
        for r in csv.DictReader(f):
            if r["module"].lower() == "sound.dll":
                rows[int(r["addr"], 16)] = r
    holds = set()
    with open(ROOT / "re" / "c3_hold.tsv", newline="") as f:
        for r in csv.reader(f, delimiter="\t"):
            if len(r) > 1 and r[0].lower() == "sound.dll":
                try:
                    holds.add(int(r[1], 16))
                except ValueError:
                    pass
    th = thunk_table(data, secs)
    out, cls = [], collections.Counter()
    for at, t in th:
        r = rows.get(t)
        name = r["name"] if r else f"FUN_{ibase + t:08x}"
        ported = ("snd." + name) in fns or ("snd." + name.split("::")[-1]) in fns
        if not ported and r is None:
            ported = (f"snd.FUN_{ibase + t:08x}") in fns
        if ported:
            c = "PORTED"
        elif r is None:
            c = "NO_ROW"
        else:
            st = (r.get("status") or "").strip()
            c = "HELD" if t in holds else (st.upper() if st in ("dead", "trivial") else f"NOT_PORTED_{r['confidence']}")
        cls[c] += 1
        out.append((f"0x{at:x}", f"0x{t:x}", name, r["confidence"] if r else "", c))
    with open(ROOT / "log" / "roadmap" / "S4" / "10_thunk_survey.tsv", "w", newline="") as f:
        w = csv.writer(f, delimiter="\t")
        w.writerow(["thunk_rva", "target_rva", "name", "level", "class"])
        w.writerows(out)
    print(f"thunks {len(th)} ({th[0][0]:#x}..{th[-1][0]:#x}): {dict(cls)}")


if __name__ == "__main__":
    main()
