"""I5: apply every shared-file edit of the S2 wave 1-4 and S3 wave 1 merge requests to <root> (the mirror or the shared tree).

Texts are taken from the merge requests themselves (fenced blocks after their headings, table rows, the uncertain TSVs, the patches),
so nothing is retyped. Each edit is anchored and refuses to apply twice (a second run on the same root stops at the first edit).

  ~/bin/py -3.12 log/roadmap/merges/S2S3-5/apply_shared.py <root> [--dry]

Order: S2 wave 1 (MERGE.md 1.1-1.4), wave 2 (3_shared.patch, U-1220..1226, CLAUDE.md), wave 3 (4_shared.patch, U-1230..1234,
CLAUDE.md), wave 4 (scenario.py glob, U-1250..1253, CLAUDE.md), S3 wave 1 (portsim_c.cpp / .h, 3_tick.patch, U-1120 / U-1076,
U-1300..1304 + U-1390..1392, PHASE5_TERRAIN_PLAN.md items 19-20, CLAUDE.md). CLAUDE.md paragraphs go after the Roadmap S0 paragraph
in that order (each request says `after the S0 / previous S2 paragraph`). UNCERTAINTIES.md rows go after the last table row; address
cells that are not hex addresses are left empty (re_classify norm_addr).
scenario_refs.json (wave 4 item 1.2) is not edited here: the shared copy already holds cov_r_s2_screens (and only that, plus its
`updated` stamp) and mirror.py copies it."""
import importlib.util
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[3]
RM = REPO / "log" / "roadmap"


def fenced_after(md: str, heading: str) -> str:
    i = md.index(heading)
    m = re.compile(r"```[a-z]*\n(.*?)\n```", re.S).search(md, i)
    assert m, heading
    return m.group(1)


def table_rows(md: str, heading: str, prefix: str) -> list:
    i = md.index(heading)
    j = md.find("\n### ", i + 1)
    seg = md[i:j if j > 0 else len(md)]
    return [l for l in seg.splitlines() if l.startswith(prefix)]


def tsv_rows(p: pathlib.Path) -> list:
    out = []
    lines = p.read_text(encoding="utf-8").splitlines()
    hdr = lines[0].split("\t")
    assert hdr == ["id", "address", "kind", "description", "needed", "status"], (p, hdr)
    for l in lines[1:]:
        if not l.strip():
            continue
        c = l.split("\t")
        assert len(c) == 6 and "|" not in l, (p, l[:60])
        out.append("| " + " | ".join(c) + " |")
    return out


def addr_cell(row: str) -> str:
    """empty the address cell when it is not a hex address."""
    cells = row.split(" | ")
    a = cells[1].strip()
    if a and not re.fullmatch(r"0x[0-9a-fA-F]+", a):
        cells[1] = ""
    return " | ".join(cells)


class Root:
    def __init__(self, root: pathlib.Path, dry: bool):
        self.root, self.dry, self.cache = root, dry, {}

    def get(self, rel):
        if rel not in self.cache:
            self.cache[rel] = (self.root / rel).read_bytes().decode("utf-8")
        return self.cache[rel]

    def put(self, rel, text):
        self.cache[rel] = text

    def nl(self, rel):
        return "\r\n" if "\r\n" in self.get(rel) else "\n"

    def conv(self, rel, s):
        s = s.replace("\r\n", "\n")
        return s.replace("\n", "\r\n") if self.nl(rel) == "\r\n" else s

    def replace(self, rel, old, new, what):
        t = self.get(rel)
        o, n = self.conv(rel, old), self.conv(rel, new)
        if t.count(o) != 1:
            # a file with mixed line ends (an earlier merge wrote LF lines into a CRLF file): take the anchor's own line ends
            for eol in ("\n", "\r\n"):
                oo = old.replace("\r\n", "\n").replace("\n", eol)
                if t.count(oo) == 1:
                    o, n = oo, new.replace("\r\n", "\n").replace("\n", eol)
                    break
        if t.count(n) and not t.count(o):
            raise SystemExit(f"{what}: already applied in {rel}")
        if t.count(o) != 1:
            raise SystemExit(f"{what}: anchor found {t.count(o)} times in {rel}")
        self.put(rel, t.replace(o, n))
        print("edit", rel, what)

    def append(self, rel, block, what, sep_blank=True):
        t = self.get(rel)
        b = self.conv(rel, block)
        if b.strip() and b.strip() in t:
            raise SystemExit(f"{what}: already applied in {rel}")
        nl = self.nl(rel)
        if not t.endswith(nl):
            t += nl
        self.put(rel, t + (nl if sep_blank else "") + b + nl)
        print("append", rel, what)

    def unc_rows(self, rows, what):
        rel = "UNCERTAINTIES.md"
        t = self.get(rel)
        nl = self.nl(rel)
        ids = [r.split("|")[1].strip() for r in rows]
        for i in ids:
            if re.search(r"^\| " + re.escape(i) + r" \|", t, re.M):
                raise SystemExit(f"{what}: {i} already in UNCERTAINTIES.md")
        lines = t.split(nl)
        last = max(k for k, l in enumerate(lines) if l.startswith("| U-"))
        lines[last + 1:last + 1] = rows
        self.put(rel, nl.join(lines))
        print("rows", rel, what, ids[0], "..", ids[-1], len(rows))

    def unc_edit_row(self, uid, fn, what):
        rel = "UNCERTAINTIES.md"
        t = self.get(rel)
        nl = self.nl(rel)
        lines = t.split(nl)
        k = [i for i, l in enumerate(lines) if l.startswith(f"| {uid} |")]
        assert len(k) == 1, (uid, k)
        new = fn(lines[k[0]])
        assert new != lines[k[0]], what
        lines[k[0]] = new
        self.put(rel, nl.join(lines))
        print("row edit", uid, what)

    def claude_after_s0(self, block, what):
        rel = "CLAUDE.md"
        t = self.get(rel)
        b = self.conv(rel, block)
        if b.split(self.nl(rel))[0] in t:
            raise SystemExit(f"{what}: already in CLAUDE.md")
        anchor = self.conv(rel, "Phase 7 defect decisions, section S6 of the roadmap orchestrator")
        # insert before the S6 paragraph, after every paragraph this script already put there (keeps their order)
        i = t.index(anchor)
        assert t.count(anchor) == 1
        self.put(rel, t[:i] + b + self.nl(rel) + t[i:])
        print("claude", what)

    def patch(self, patch, what):
        # git apply relative to the repo root with --directory = root's path inside the repo; the files must be
        # the cached state, so flush first
        self.flush()
        rel_root = self.root.resolve().relative_to(REPO.resolve()).as_posix()
        args = ["git", "apply", "-v"] + ([f"--directory={rel_root}"] if rel_root != "." else [])
        chk = subprocess.run(args + ["--check", str(patch)], cwd=REPO, capture_output=True, text=True)
        print(chk.stdout + chk.stderr, end="")
        if chk.returncode:
            raise SystemExit(f"{what}: git apply --check failed")
        if not self.dry:
            subprocess.run(args + [str(patch)], cwd=REPO, check=True)
        self.cache.clear()
        print("patch", patch.name, what)

    def flush(self):
        if self.dry:
            return
        for rel, t in self.cache.items():
            p = self.root / rel
            if p.read_bytes().decode("utf-8") != t:
                p.write_bytes(t.encode("utf-8"))
                print("wrote", rel)


def load_s3_mirror():
    spec = importlib.util.spec_from_file_location("s3_1_mirror", RM / "S3" / "1_mirror.py")
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def main():
    root = pathlib.Path(sys.argv[1]).resolve()
    dry = "--dry" in sys.argv[2:]
    if dry:
        sys.exit("--dry cannot preview the patches (they apply to disk); run on a scratch copy instead")
    R = Root(root, dry)
    m1 = (RM / "S2" / "MERGE.md").read_text(encoding="utf-8")
    m2 = (RM / "S2" / "MERGE_wave2.md").read_text(encoding="utf-8")
    m3 = (RM / "S2" / "MERGE_wave3.md").read_text(encoding="utf-8")
    m4 = (RM / "S2" / "MERGE_wave4.md").read_text(encoding="utf-8")
    s3 = (RM / "S3" / "MERGE.md").read_text(encoding="utf-8")

    # ---- S2 wave 1
    R.replace("port/test/test_static.py",
              '    files = {p.stem for p in (PORT / "src" / "sim").glob("*.cpp")}\n',
              '    # gfx body families (roadmap S2.1, port/src/sim/gfx_body.h): gfx_<f>.cpp files listed in gfx_families.inc, not modules\n'
              '    families = set(re.findall(r"^SG_GFX_FAMILY\\((\\w+)\\)", (PORT / "src" / "sim" / "gfx_families.inc").read_text(), re.M))\n'
              '    for f in families:\n'
              '        src = (PORT / "src" / "sim" / f"gfx_{f}.cpp").read_text(encoding="utf-8")\n'
              '        assert f"kGfxFamily_{f}" in src and f"g_negctl_gfx_{f}" in src, f\n'
              '    files = {p.stem for p in (PORT / "src" / "sim").glob("*.cpp")} - {f"gfx_{f}" for f in families}\n',
              "S2 1.1 module listing")
    R.append("port/test/README.md", fenced_after(m1, "### 1.2 port/test/README.md"), "S2 1.2 body families")
    R.unc_rows([addr_cell(r) for r in table_rows(m1, "### 1.3 UNCERTAINTIES.md", "| U-12")], "S2 wave 1")
    R.claude_after_s0(fenced_after(m1, "### 1.4 CLAUDE.md"), "S2 wave 1")
    # ---- S2 wave 2
    R.patch(RM / "S2" / "3_shared.patch", "S2 wave 2")
    R.unc_rows([addr_cell(r) for r in tsv_rows(RM / "S2" / "3_uncertain.tsv")], "S2 wave 2")
    R.claude_after_s0(fenced_after(m2, "## 4. CLAUDE.md paragraph"), "S2 wave 2")
    # ---- S2 wave 3
    R.patch(RM / "S2" / "4_shared.patch", "S2 wave 3")
    R.unc_rows([addr_cell(r) for r in tsv_rows(RM / "S2" / "4_uncertain.tsv")], "S2 wave 3")
    R.claude_after_s0(fenced_after(m3, "## 4. CLAUDE.md paragraph"), "S2 wave 3")
    # ---- S2 wave 4
    R.replace("re/tools/scenario.py",
              'for _p in sorted(_glob.glob(str(pathlib.Path(__file__).with_name("scenario_s[18]_*.py")))):\n',
              'for _p in sorted(_glob.glob(str(pathlib.Path(__file__).with_name("scenario_s[0-9]*_*.py")))):\n',
              "S2 wave 4 1.1 hook glob")
    R.unc_rows([addr_cell(r) for r in table_rows(m4, "### 1.4 UNCERTAINTIES.md", "| U-125")], "S2 wave 4")
    R.claude_after_s0(fenced_after(m4, "### 1.5 CLAUDE.md"), "S2 wave 4")
    # ---- S3 wave 1
    mm = load_s3_mirror()
    R.replace("port/src/portsim_c.cpp", mm.OLD_RENDER, mm.NEW_RENDER, "S3 1a sg_gl_render")
    R.replace("port/src/portsim_c.cpp", mm.OLD_RASTER, mm.NEW_RASTER, "S3 1a sg_gl_render_to_raster")
    R.replace("port/include/portsim_c.h", mm.OLD_DOC, mm.NEW_DOC, "S3 1b doc")
    R.patch(HERE / "3_tick_paths.patch", "S3 1c tick.cpp (3_tick.patch, file headers rewritten)")
    R.unc_edit_row("U-1120", lambda l: _resolve(l, "Resolved by roadmap S3.3: the -11 was NeedInput (a MissingResource) thrown by the boot's last frame's Blocking pump when no recorded input is left (boot runs with to_end), not the pending snapshot; run_frames now catches it there and in the end-checkpoint block and calls gfx_end_of_input (log/roadmap/S3/3_notes.md)."),
                   "S3 2 U-1120 resolved")
    R.unc_edit_row("U-1076", lambda l: _append_desc(l, "S3.1b: the specular table (U-1390) removes most of the one-level noise this row describes (c7 mean 0.46 % -> 0.06 %, t3 0.49 % -> 0.05 %); the per-triangle residual must be re-measured with it (U-1392)."),
                   "S3 3b U-1076")
    R.unc_rows(table_rows(s3, "### 2. UNCERTAINTIES.md", "| U-1"), "S3 wave 1")
    plan = fenced_after(s3, "### 3. re/PHASE5_TERRAIN_PLAN.md")
    R.append("re/PHASE5_TERRAIN_PLAN.md", plan, "S3 3 items 19-20")
    R.claude_after_s0(fenced_after(s3, "### 4. CLAUDE.md"), "S3 wave 1")
    R.flush()
    print("done", root)


def _resolve(row: str, text: str) -> str:
    cells = row.split(" | ")
    assert len(cells) == 6 and cells[5] == "open |", cells[5]
    cells[3] = cells[3].rstrip() + " " + text
    cells[5] = "resolved |"
    return " | ".join(cells)


def _append_desc(row: str, text: str) -> str:
    # columns: | id | addr | type | description | path | status |
    cells = row.split(" | ")
    assert len(cells) == 6, len(cells)
    cells[3] = cells[3].rstrip() + " " + text
    return " | ".join(cells)


if __name__ == "__main__":
    main()
