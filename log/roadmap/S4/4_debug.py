"""S4.4 debugging aid: one key through the oracle (layout A), printing per vector the port error, the core's detail, the x87
detail and the first differing region bytes."""
import sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import test_audio_mixer as T
S, E = T.S, T.E
T.use_mirror()
key = sys.argv[1]
only = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else (-1 if len(sys.argv) > 2 else None)
lib = S.load_lib()
if key in S.DERIVED:
    E.registry()[0][key] = S.spec_of(key)
with E.FixVM() as vm:
    lp = T.MixerLivePort(lib, key, True)
    try:
        res = E.KeyRun(vm, key, "A", second_arm=lp).run()
    finally:
        lp.close()
    regions = res.regions
    for i, (row, rec) in enumerate(zip(res.rows, lp.records)):
        if only is not None and i != only:
            continue
        print(f"v{i} match {row.match} orig {row.orig} port {row.re} err {row.error}")
        print("   report", rec.report.get("detail"), "fallback_rvas", rec.report.get("fallback_rvas"), "x87:", getattr(rec, "x87_detail", ""))
        if getattr(rec, "x87_diff", ""):
            print("   x87diff", rec.x87_diff)
        if row.state_hex and row.state_re_hex and row.state_hex != row.state_re_hex:
            a, b = row.state_hex.split("|"), row.state_re_hex.split("|")
            for k, ((va, n), x, y) in enumerate(zip(regions, a, b)):
                if x != y:
                    j = next(t for t in range(0, len(x), 2) if x[t:t+2] != y[t:t+2])
                    print(f"   region {k} va 0x{va:x}+0x{n:x}: first diff byte +0x{j//2:x}: emu {x[j:j+32]} port {y[j:j+32]}")
                    break
if len(sys.argv) > 3:
    for i, rec in enumerate(lp.records):
        a = T.x87_view(rec.x87_in); b = T.x87_view(rec.x87_exp)
        print(f"v{i} in cw {a['cw']:x} top {a['top']} use {sorted(a['regs'])}  out cw {b['cw']:x} top {b['top']} use {sorted(b['regs'])}")
if len(sys.argv) > 4:
    rec = lp.records[only]
    for e in rec.script:
        print("   host", e["kind"], hex(e["va"]), "args", [hex(x) for x in e["args"]], "eax", hex(e["eax"]), "writes", len(e["writes"]))
if len(sys.argv) > 2 and sys.argv[2] == "summary":
    for i, rec in enumerate(lp.records):
        b = T.x87_view(rec.x87_exp)
        print(f"v{i} dispatched {rec.report.get('dispatched')} claimed {rec.report.get('claimed')} external {rec.report.get('external')} "
              f"exit top {b['top']} in-use {len(b['regs'])} cc {b['cc']:#x} cw {b['cw']:#x}")
