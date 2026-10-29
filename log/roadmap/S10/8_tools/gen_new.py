"""S10.8: write re/names/smsg_exe_1.tsv (new rows for FUN_ functions) from the evidence logs of this step."""
import pathlib, re
R = pathlib.Path(__file__).resolve().parents[4]
L = R / "log/roadmap/S10"
rows = [("golf_clean.exe", "0x0047e770", "View::drawSlot78", "ui",
         "slot 78 (+0x138) of the View vtable 0x004ba8b4 (dword 0x004ba9ec; installers View_ctor 0x00479c40, View_dtor 0x0047a3c0, "
         "re/tools/pointer_callers.py); no direct caller; callees are all drawing: Widget_fillRectR 0x00475b00, Surface_blitThunk 0x00475c60, "
         "blitMid 0x00475fb0, Win_fillRect 0x00478af0, Widget_fillArea 0x00478b50 (re/tools/xref.py); golf.pdb names it WidgetBase::on_paint",
         "View virtual slot 78: fills rectangles and blits the View's surface parts; two idiv take divisors from child slots +0xd8 / +0xdc "
         "with no zero test (0x0047e8e1, 0x0047e8f7). Which View state it paints is not determined")]
crt = re.compile(r"^(0x[0-9a-f]+) (\S+): member (\S+): ([\d.]+)% \((\d+)/(\d+)\)")
for l in (L / "8_crtlib.log").read_text(encoding="utf-8").splitlines():
    m = crt.match(l)
    if not m:
        continue
    va, sym, obj, pct, k, n = m.groups()
    name = sym.split("@@")[0].lstrip("?") if sym.startswith("?") else sym
    obj = obj.split("\\")[-1]
    if pct == "100.0":
        ev = (f"library identity: all {n} instructions equal libcmt.lib member {obj}'s {sym} (VC6 12.00.8168, "
              f"tools/vc6/vc98/lib/libcmt.lib) with log/c3/m36e_lib_match.py, evidence CSV log/roadmap/S10/8_crtlib")
    else:
        ev = (f"library identity: the {k} instructions of the master's extent equal the first {k} of libcmt.lib member {obj}'s {sym} "
              f"({k}/{n}); the library body continues {int(n) - int(k)} instructions past the extent re/functions_ghidra.tsv records "
              f"(log/c3/m36e_lib_match.py, CSV log/roadmap/S10/8_crtlib)")
    rows.append(("golf_clean.exe", f"0x{int(va, 16):08x}", name, "crt", ev,
                 f"VC6 static CRT {name} ({obj}), linked unchanged"))
inst = {}
for l in (L / "8_jpeg_installers.txt").read_text(encoding="utf-8").splitlines():
    p = l.split()
    if len(p) >= 5:
        inst[int(p[0], 16)] = f"stored at {p[1]} by {p[4]} {p[3]}"
src, best = None, {}
for l in (L / "8_jpeg_autoname.log").read_text(encoding="utf-8").splitlines():
    if l.startswith("== "):
        src = l[3:].strip()
    m = re.match(r"// MATCH: golf_clean.exe (0x[0-9a-f]+) _(\w+)", l)
    if m:
        best[int(m.group(1), 16)] = (m.group(2), src, "100%")
    m = re.match(r"#\s+(0x[0-9a-f]+) best ([\d.]+%) _(\w+)", l)
    if m:
        best[int(m.group(1), 16)] = (m.group(3), src, m.group(2))
for va in sorted(best):
    name, s, pct = best[va]
    f = "log/roadmap/S10/8_tools/jpeg_fdct.cpp" if s == "fdct" else f"re/match/{s}.cpp"
    ev = f"re/tools/match_autoname.py: {pct} against IJG libjpeg 6a compiled by {f} (/O2, C)"
    if va in inst:
        ev += f"; {inst[va]} (re/tools/pointer_callers.py)"
    if va == 0x4b5d50:
        ev += ("; jinit_forward_dct 0x004b34b0 (100% in re/match/golf_jpeg_cdctmgr.cpp) stores it as do_dct at 0x004b351c in the case "
               "that pairs forward_DCT 0x004b3720 with it, beside jpeg_fdct_ifast 0x004b6110 at 0x004b350c and jpeg_fdct_float 0x004b6360 "
               "at 0x004b34fc; its own body does not match (U-1885)")
    if va == 0x4b4ca0:
        ev += ("; the one unequal instruction reads `mov dword ptr [esp + 0x1c], 0x807fff` on both sides (re/tools/match.py --show, "
               "log/roadmap/S10/8_jpeg_probe.log): the immediate CBCR_OFFSET + ONE_HALF - 1 falls in the image range, which match.py "
               "masks as an address (U-1886); stored at 0x004b4b26 / 0x004b4ba0 / 0x004b4c1c by jinit_color_converter 0x004b4a60")
    rows.append(("golf_clean.exe", f"0x{va:08x}", name, "util", ev, f"IJG libjpeg 6a static function {name}"))
hdr = "module\taddr\tname\tsubsystem\tevidence\tpurpose"
(R / "re/names/smsg_exe_1.tsv").write_text(hdr + "\n" + "\n".join("\t".join(r) for r in rows) + "\n", encoding="utf-8")
print(len(rows), "rows")
