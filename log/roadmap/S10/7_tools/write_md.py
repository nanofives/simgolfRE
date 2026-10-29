"""S10.7: re/analysis/systems/smsg_<exe|jgld|sound|terrain>_lib.md from the score and ledger files.
~/bin/py -3.12 log/roadmap/S10/7_tools/write_md.py"""
import collections, pathlib

ROOT = pathlib.Path(__file__).resolve().parents[4]
INFO = {
    "golf": ("smsg_exe_lib", "golf_clean.exe", "VC6 RTM libcmt.lib (tools/vc6/vc98/lib), also libcimt / libcpmt",
             "the exe links the RTM static CRT: of the library procedures with a member in libcmt, the RTM archive is identical for every one except __amsg_exit, SP5 and SP2 for fewer (7_agree_golf.tsv)"),
    "jgld": ("smsg_jgld_lib", "jgld.dll", "VC6 SP2 libcmtd.lib (tools/vc6sp2/VC98/lib/lib), also LIBCIMTD / LIBCPMTD",
             "jgld.pdb names C:\\VS6SP2\\VC98\\LIB\\libcmtd.lib as the archive of 319 procedures"),
    "sound": ("smsg_sound_lib", "sound.dll", "VC6 SP5 libcmt.lib (tools/vc6sp5/VC98/lib), also libcimt / libcpmt",
              "sound.pdb has no archive module for the CRT, so the candidates are PDB publics outside every procedure; SP5 is identical for 393 of the 394 libcmt candidates, SP2 for 391, RTM for 368 (7_agree_sound.tsv)"),
    "Terrain": ("smsg_terrain_lib", "Terrain.dll", "VC6 SP2 libcmtd.lib, libcimtd.lib",
                "Terrain.pdb names libcmtd.lib (340 procedures) and libcimtd.lib (133)"),
}
for key, (nid, mod, lib, why) in INFO.items():
    led = [l.split("\t") for l in (ROOT / f"log/roadmap/S10/7_ledger_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]]
    cnt = collections.Counter(c[1] for c in led)
    sc = [l.split("\t") for l in (ROOT / f"log/roadmap/S10/7_score_{key}.tsv").read_text(encoding="utf-8").splitlines()[1:]]
    var = collections.Counter((c[8] or "extent as listed") for c in sc if float(c[5]) == 100.0)
    lines = [f"# {nid}: {mod} static CRT routines by library-object identity (S10.7, 2026-10-29)", "",
             f"Archive: {lib}. Why this archive: {why}.", "",
             "A row is accepted when the library member's function object is identical to our body: match.py's normalized",
             "instruction comparison at 100% (relocated operands masked), or, where jump tables sit inside the body",
             "(memcpy, memmove), equal bytes with the relocation sites masked and each section-local relocation holding",
             "address + label + stored addend. No compilation is involved. Driver: log/roadmap/S10/7_tools/libscore.py.", "",
             f"Rows here: {cnt['ACCEPT'] + cnt['ACCEPT_REV']} ({cnt['ACCEPT_REV']} of them found by comparing leftover functions with every library body, no PDB name). Skipped: {cnt['HAS_ROW']} addresses already have a row in another names file,",
             f"{cnt['NOT_FUNCTION']} are entry labels or thunks that no function of ours starts at.", "",
             "Extent used for the 100% result (how many addresses):"]
    lines += [f"- {k}: {v}" for k, v in sorted(var.items())]
    lines += ["", "Names: C routines keep the PDB public name without the @N suffix (S10.8 convention) except jgld.dll and",
              "Terrain.dll, which take the PDB's undecorated name (S10.9 convention). C++ names read Class::member, and a",
              "name already used in the module gets the address as a suffix.", "",
              "CSVs: log/roadmap/S10/7_lib/" + key + "/<archive tag>/ (never log/diff; the copy step is in 7_merge.md).", ""]
    (ROOT / f"re/analysis/systems/{nid}.md").write_text("\n".join(lines), encoding="utf-8")
    print(nid, dict(cnt))
