"""S10.10: table slots holding each given exe function (pointer_callers.references), with installers.
~/bin/py -3.12 log/roadmap/S10/10_tools/slotof.py 0x48c610 0x48aa00 ..."""
import pathlib, sys
R = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(R / "re" / "tools"))
import pointer_callers as pc, xref  # noqa: E402
m = pc.mod("golf_clean.exe")
idx, nm = xref.load("golf_clean.exe"), xref.names("golf_clean.exe")
for a in sys.argv[1:]:
    va = int(a, 16)
    refs = pc.references(m, va)
    print(f"0x{va:08x} {xref.label(va, idx, nm)}: " + "; ".join(
        f"{d} [{', '.join(xref.label(x, idx, nm) for x in inst)}]" for _, _, d, inst in refs))
