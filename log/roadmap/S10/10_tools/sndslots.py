"""S10.10: bodies at byte offsets of the two sound.dll classes an exe Snd / VoiceRx object holds at +0x40
(MultiSound 0x1005c7e8 for createSound type 1, the 0x1005c450 class for type 6; see fwd2.py), with our names and
sound.pdb's. ~/bin/py -3.12 log/roadmap/S10/10_tools/sndslots.py 0x14 0x80 ..."""
import pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import fwd2  # noqa: E402  (rewrites 10_fwd.tsv as a side effect; deterministic)

for o in sys.argv[1:]:
    off = int(o, 16)
    cells = []
    for vt in (fwd2.MULTI, fwd2.VRX):
        t = fwd2.body(fwd2.sdw(vt + off))
        cells.append(f"0x{t:08x} {fwd2.names.get(t, '-')} [{'|'.join(sorted(fwd2.sp.get(t, {'-'})))}]")
    print(f"+{o} slot {off // 4}: " + "  ||  ".join(cells))
