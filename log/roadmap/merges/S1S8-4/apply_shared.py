"""I4 (S1 + S8 merge): the shared-file edits of log/roadmap/S1/MERGE.md (apply order 1-8) and log/roadmap/S8/MERGE.md (items 1-7, the
optional 4 and 6 included) plus both CLAUDE.md paragraphs, applied to a tree root (the mirror first, then the shared tree). Every edit
is anchored: the anchor must occur exactly once and the edit must not be present yet (the script refuses to apply twice).

S1 step 1 (log/roadmap/S1/tick_combined.diff) is applied with `git apply --ignore-whitespace` in a scratch directory OUTSIDE the repo
(the mirror lies inside the shared repo's working tree, where git apply would resolve paths against the shared repo) on the root's six
files, which are then written back. When the root already holds the diff (a reverse check passes), the step is skipped.

re/port/divergences.tsv (S1 item 6) is not written here: `port.test.integrator ledger` regenerates it (see S1S8-4.md).
Usage: ~/bin/py -3.12 log/roadmap/merges/S1S8-4/apply_shared.py <root>"""
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROADMAP = pathlib.Path(__file__).resolve().parents[2]   # log/roadmap (log is shared through the mirror's junction)
S1 = ROADMAP / "S1"
S8 = ROADMAP / "S8"
root = pathlib.Path(sys.argv[1]).resolve()
DATE = "2026-10-24"


def rd(p):
    return (root / p).read_bytes().decode("utf-8")


def wr(p, s):
    (root / p).write_bytes(s.encode("utf-8"))


def edit(p, old, new, what):
    """Replace `old` once. The texts are written with LF; when the LF form does not occur, the CRLF form is tried (the shared
    tree holds LF and CRLF files, the mirror's git archive CRLF only)."""
    t = rd(p)
    forms = ((old, new), (old.replace("\n", "\r\n"), new.replace("\n", "\r\n")))
    for o, n in forms:
        c = t.count(o)
        if c == 1:
            wr(p, t.replace(o, n, 1))
            print(f"edited {p}: {what}")
            return
        if c > 1:
            sys.exit(f"{p} ({what}): anchor occurs {c} times")
    if any(n and n in t for _, n in forms):
        sys.exit(f"{p} ({what}): already applied")
    sys.exit(f"{p} ({what}): anchor not found")


# ----------------------------------------------------------------------------------------------------- S1 step 1: the diff
DIFF = S1 / "tick_combined.diff"
SIX = ["port/src/sim/tick.cpp", "port/include/portsim/sim/commands.h", "port/src/sim/commands.cpp",
       "port/src/sim/commands_internal.h", "port/src/sim/menu.cpp", "port/include/portsim/sim/chareditor.h"]
with tempfile.TemporaryDirectory() as td:
    td = pathlib.Path(td)
    for f in SIX:
        (td / f).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / f, td / f)
    rev = subprocess.run(["git", "apply", "--check", "-R", "--ignore-whitespace", str(DIFF)], cwd=td, capture_output=True, text=True)
    if rev.returncode == 0:
        print("S1 step 1: tick_combined.diff already applied in this root (reverse check passes), skipped")
    else:
        r = subprocess.run(["git", "apply", "--ignore-whitespace", str(DIFF)], cwd=td, capture_output=True, text=True)
        if r.returncode:
            sys.exit("S1 step 1: git apply failed:\n" + r.stderr)
        for f in SIX:
            shutil.copyfile(td / f, root / f)
        print("S1 step 1: tick_combined.diff applied (6 files)")

# ----------------------------------------------------------------------------------- S1 step 2: the Load-pump marker goes
edit("port/test/test_s1_4_scenarios.py",
     "# U-1051: a message posted before a world load is dispatched by loadFlic's pumpInput483bd0 (+0x54). The port takes it with\n"
     "# PumpKind::Load (patch text in log/roadmap/S1/4_merge.md, not applied to the shared tree): these tests need a DLL built\n"
     "# with it (SG_S14_LOAD_PUMP=1 with SG_PORTSIM_DLL pointing at that build) and are skipped otherwise. Without the patch the boot\n"
     "# run reads RED at record 1 (g_cachedCameraTileX: the original's first frame takes the 'z' and zooms).\n"
     "FLIC = \"cov_r_flic_input\"\n"
     "needs_load_pump = pytest.mark.skipif(os.environ.get(\"SG_S14_LOAD_PUMP\") != \"1\",\n"
     "                                     reason=\"needs a build with the Load pump (log/roadmap/S1/4_merge.md)\")\n",
     "# U-1051: a message posted before a world load is dispatched by loadFlic's pumpInput483bd0 (+0x54). The port takes it with\n"
     "# PumpKind::Load (tick.cpp TraceInput, roadmap S1.4). Without it the boot run reads RED at record 1 (g_cachedCameraTileX:\n"
     "# the original's first frame takes the 'z' and zooms), which tick.frame 8 reproduces.\n"
     "FLIC = \"cov_r_flic_input\"\n",
     "needs_load_pump marker removed")
edit("port/test/test_s1_4_scenarios.py", "@needs_load_pump\ndef test_flic_input_boot_passes(", "def test_flic_input_boot_passes(",
     "decorator 1")
edit("port/test/test_s1_4_scenarios.py", "@needs_load_pump\ndef test_flic_input_negctl_reads_red(",
     "def test_flic_input_negctl_reads_red(", "decorator 2")
t = rd("port/test/test_s1_4_scenarios.py")
assert "os." not in t.replace("import os", ""), "os still used"
edit("port/test/test_s1_4_scenarios.py", "import json\nimport os\nimport struct\n", "import json\nimport struct\n", "unused import os")

# -------------------------------------------------------------------------- S1 step 4: test_boot NEGCTL + docstring (3_merge.md)
edit("port/test/test_boot.py",
     "          (\"cov_b_title\", \"showThemePackSelect\", 1, \"a row press does not change g_theme\"),\n",
     "          (\"cov_b_title\", \"showThemePackSelect\", 1, \"a row press does not change g_theme\"),\n"
     "          (\"cov_r_theme_cancel\", \"showThemePackSelect\", 2, \"Cancel keeps the theme\"),\n",
     "NEGCTL cov_r_theme_cancel")
edit("port/test/test_boot.py", "the theme-pack row press, the window procedure's", "the theme-pack row press and Cancel, the window procedure's",
     "docstring: Cancel listed")
edit("port/test/test_boot.py",
     "returns after the camera reset), showThemePackSelect 2 (no\nscenario cancels the theme screen), loadInterfaceArt 3",
     "returns after the camera reset), loadInterfaceArt 3",
     "docstring: theme sentence removed")

# ------------------------------------------------------------------------------------------- S1 step 6: chooseShotTarget evidence
edit("re/analysis/data/model_defects.md",
     "(the port starts it at 0, U-0880; switch chooseShotTarget.strictFlagSticky) | set 0x00423b0e, read 0x00423b20; no other write in "
     "0x00422fb0..0x004240e8 | log/p3/p34_notes.txt 3 |",
     "(the port starts it at 0, U-0880; switch chooseShotTarget.strictFlagSticky) | set 0x00423b0e, read 0x00423b20; no other write in "
     "0x00422fb0..0x004240e8 | log/p3/p34_notes.txt 3; evidence port/test/test_shots_strict.py (roadmap S1.2, U-1012 resolved) |",
     "row 37 evidence")
edit("port/test/integrator.py",
     "                 \"shotPower.cacheIgnoresPutt\": \"port/test/test_shots.py::test_defect_shot_power_putt (vectors)\",\n",
     "                 \"shotPower.cacheIgnoresPutt\": \"port/test/test_shots.py::test_defect_shot_power_putt (vectors)\",\n"
     "                 # roadmap S1.2 (U-1012): constructed states where the original keeps the strict flag; the original in Unicorn\n"
     "                 # with SG_EMU_ORACLE=1 (test_original_equals_port_with_the_flag_sticky), OFF RED 8 / 8\n"
     "                 \"chooseShotTarget.strictFlagSticky\": \"port/test/test_shots_strict.py::test_the_switch_changes_the_search, \"\n"
     "                                                      \"test_golden_digests_of_the_original (state)\",\n",
     "ledger evidence map: strictFlagSticky")
edit("port/test/test_shots.py",
     "    failing when the corpus grows: then turn this into a real ON / OFF test.\"\"\"\n",
     "    failing when the corpus grows: then turn this into a real ON / OFF test. The switch itself is judged by the constructed\n"
     "    states of port/test/test_shots_strict.py (roadmap S1.2, U-1012).\"\"\"\n",
     "docstring pointer to test_shots_strict.py")

# ------------------------------------------------------------------------------------------------------- S8 items 1-6
t = rd("port/src/sim/modules.inc")
if "SG_MODULE(video)" in t:
    sys.exit("modules.inc: video already listed")
eol = "\r\n" if "\r\n" in t else "\n"
wr("port/src/sim/modules.inc", t.rstrip("\r\n") + eol + "SG_MODULE(video)" + eol)
print("edited port/src/sim/modules.inc: SG_MODULE(video)")
(root / "port/src/registry.cpp").touch()
print("touched port/src/registry.cpp")

edit("port/src/sim/tick.cpp",
     "    {\"playAudioFile\", \"the closing Bink video (binkw32); its pumps dispatch no recorded input in the corpus\"},\n", "",
     "kDrops: playAudioFile removed (S8 item 2)")
edit("port/src/sim/tick_boot.inc", "    cc(\"showTitleScreen\");\n",
     "    // gameMain+0x41f showTitleScreen plays the intro first (its call at showTitleScreen+0x27, immediate 0): the video\n"
     "    // module (S8.3), whose default host takes the original's own skip path (BinkOpen NULL)\n"
     "    cc(\"playAudioFile\", (GS(f_strpool_4c89ac) + 96), 0);           // ./flics/SMSG_introfinal.bik\n"
     "    cc(\"showTitleScreen\");\n",
     "BootGameMain: the intro call (S8 item 3)")
edit("port/src/sim/tick.cpp",
     "      tk::FrameIoStats io;\n"
     "      try {\n"
     "        tk::tick_frame_io(ctx, in, io);\n"
     "      } catch (const NeedInput &) {\n",
     "      tk::FrameIoStats io;\n"
     "      FrameExit fex = FrameExit::Menu;\n"
     "      try {\n"
     "        fex = tk::tick_frame_io(ctx, in, io);\n"
     "      } catch (const NeedInput &) {\n",
     "frameless boot branch keeps the FrameExit (S8 item 4)")
edit("port/src/sim/tick.cpp",
     "      if (detail) *detail = \"trace has no FRAME record after the boot; the boot ran to the end of the trace\";\n",
     "      if (detail) *detail = fex == FrameExit::Quit ? \"trace has no FRAME record after the boot; mainLoop returned (the title menu's quit)\"\n"
     "                                                   : \"trace has no FRAME record after the boot; the boot ran to the end of the trace\";\n",
     "frameless boot branch reports the quit (S8 item 4)")
edit("re/tools/scenario.py",
     "# Roadmap S1 (log/roadmap/S1): each step worker registers its own scenarios from re/tools/scenario_s1_<k>.py\n",
     "# Roadmap S1 and S8 (log/roadmap/S1, S8): each step worker registers its own scenarios from re/tools/scenario_s<n>_<k>.py\n",
     "hook comment")
edit("re/tools/scenario.py", "with_name(\"scenario_s1_*.py\")", "with_name(\"scenario_s[18]_*.py\")", "hook glob scenario_s[18]_*.py (S8 item 5)")
edit("re/tools/trace_tool.py",
     "    if mode == \"replay\":\n"
     "        polls = state[\"polls\"]\n"
     "\n"
     "        def replay_script(c):\n"
     "            for name in polls[:-1]:\n"
     "                if name:\n"
     "                    yield from c.checkpoint(name)\n"
     "                else:\n"
     "                    yield\n"
     "        S.SCENARIOS[scenario] = replay_script\n",
     "    if mode == \"replay\":\n"
     "        polls = state[\"polls\"]\n"
     "        # U-1714 (roadmap S8.2): a scenario that ends with the game's own exit (its lockstep function has expect_exit) is\n"
     "        # replayed expecting the exit and keeps answering polls after the recorded ones, until ExitProcess ends the trace\n"
     "        exits = getattr(S.SCENARIOS.get(scenario), \"expect_exit\", False)\n"
     "\n"
     "        def replay_script(c):\n"
     "            if exits:\n"
     "                c.run.exit_expected = True\n"
     "            for name in polls[:-1]:\n"
     "                if name:\n"
     "                    yield from c.checkpoint(name)\n"
     "                else:\n"
     "                    yield\n"
     "            if exits:\n"
     "                for _ in range(300):\n"
     "                    yield\n"
     "        S.SCENARIOS[scenario] = replay_script\n",
     "replay of an exiting scenario (S8 item 6)")

# ------------------------------------------------------------------------------------- UNCERTAINTIES.md (S1 item 5, S8 item 7)
def is_hex(a):
    try:
        int(a, 16)
        return a.lower().startswith("0x")
    except ValueError:
        return False


def tsv_rows(path, step):
    """UNCERTAINTIES.md rows from one worker TSV. Shapes met: a header `ID addr type description path status` or none (6
    columns); 4 columns whose last holds `description | path | status`; S8.1's `id address kind unknown decide`. An address cell
    that is not a hex address is left empty and its text moved to the front of the description (re_classify's norm_addr raises
    on anything else)."""
    lines = [l for l in path.read_bytes().decode("utf-8").splitlines() if l.strip()]
    head = None
    if not lines[0].startswith("U-"):
        head = [h.strip().lower() for h in lines[0].split("\t")]
        lines = lines[1:]
    out = []
    for l in lines:
        c = l.split("\t")
        if head == ["id", "address", "kind", "unknown", "decide"]:
            uid, addr, kind, desc, path_ = c
            typ, desc, status = "behavioural", f"[{step}, kind `{kind}`] {desc}", "open"
        elif len(c) == 6:
            uid, addr, typ, desc, path_, status = c
        elif len(c) == 4:
            uid, addr, typ = c[:3]
            parts = c[3].split(" | ")
            assert len(parts) == 3, (uid, len(parts))
            desc, path_, status = parts
        else:
            sys.exit(f"{path}: row {c[0]} has {len(c)} columns")
        assert uid.startswith("U-") and len(uid) == 6, uid
        if desc.startswith(step + ": "):
            desc = f"[{step}] " + desc[len(step) + 2:]
        if addr == "-":
            addr = ""
        if addr and not is_hex(addr):
            tag = f"[{step}"
            desc = desc.replace("] ", f"] {addr}: ", 1) if desc.startswith(tag) else f"{addr}: {desc}"
            addr = ""
        for x in (desc, path_, status, typ):
            assert "|" not in x.replace(" | ", "\x00") and "\x00" not in x, (uid, x[:80])
        out.append(f"| {uid} | {addr} | {typ} | {desc} | {path_} | {status} |")
    return out


rows = []
for k in (1, 2, 3, 4, 5):
    rows += tsv_rows(S1 / f"{k}_uncertain.tsv", f"S1.{k}")
for k in (1, 2, 3):
    rows += tsv_rows(S8 / f"{k}_uncertain.tsv", f"S8.{k}")
ids = [r.split(" | ")[0][2:] for r in rows]
want = (["U-1150", "U-1151", "U-1160", "U-1161", "U-1162", "U-1170", "U-1171", "U-1172"] + [f"U-118{i}" for i in range(4)]
        + [f"U-119{i}" for i in range(6)] + [f"U-170{i}" for i in range(4)] + [f"U-171{i}" for i in range(7)]
        + [f"U-172{i}" for i in range(4)])
assert ids == want, ids
u = rd("UNCERTAINTIES.md")
if "| U-1150 |" in u or "| U-1700 |" in u:
    sys.exit("UNCERTAINTIES.md: S1 / S8 rows already present")
assert u.endswith("\r\n")
u = u + "\r\n".join(rows) + "\r\n"


def set_row(u, uid, desc_old, desc_new, status_old, status_new):
    lines = u.split("\r\n")
    hit = [i for i, l in enumerate(lines) if l.startswith(f"| {uid} |")]
    assert len(hit) == 1, (uid, hit)
    l = lines[hit[0]]
    assert l.endswith(f" | {status_old} |"), (uid, l[-60:])
    l = l[: -len(f" {status_old} |")] + f" {status_new} |"
    if desc_old is not None:
        assert l.count(desc_old) == 1, (uid, desc_old[:60])
        l = l.replace(desc_old, desc_new, 1)
    lines[hit[0]] = l
    print("UNCERTAINTIES.md: row", uid, "->", status_new)
    return "\r\n".join(lines)


# U-1060 / U-1067: log/roadmap/S1/1_notes.md
u = set_row(u, "U-1060",
            "uses the bytes past the NUL. |",
            "uses the bytes past the NUL. Resolved (roadmap S1.1, 2026-10-24): the four header read sites (loadGame, loadCourse) and the "
            "two writers are pinned by port/test/test_sve_header_readers.py (re/analysis/data/sve_header_readers.md) and none reads "
            "past the NUL, so the residue is dead data (the pins are structural: U-1150). |",
            "open", f"resolved (S1.1, {DATE})")
u = set_row(u, "U-1067",
            "compares the checkpoints the run reached and said SAME. |",
            "compares the checkpoints the run reached and said SAME. Resolved (roadmap S1.1, 2026-10-24): trace_format.validate's "
            "rule undelivered_input rejects a first-generation input MSG (WM_KEYDOWN / UP, WM_SYSKEYDOWN / UP, mouse buttons) with no "
            "earlier harness DELIVER post; WM_CHAR and WM_MOUSEMOVE are derived and exempt, a slice is exempt up to its second FRAME "
            "(U-1151); 266 of 266 stored traces validate (tests/test_trace_format_s1.py). |",
            "open", f"resolved (S1.1, {DATE})")
# U-1012: log/roadmap/S1/2_merge.md item 2
u = set_row(u, "U-1012",
            "the sticky behaviour shows only when a later landing of the same search fails that condition. |",
            "the sticky behaviour shows only when a later landing of the same search fails that condition. Resolved by "
            "port/test/test_shots_strict.py: 8 constructed states (seeds of `_case`; 42 of 700 swept differ) where the original, run in "
            "Unicorn, ends with the strict flag sticky; the port ON equals it in eax and the whole image, OFF is RED (8 / 8); code hooks "
            "on 0x00423b0e / 0x00423b20 show a scan reading 1 with no store at its own landing. Residual: U-1160..U-1162. |",
            "open", f"resolved (S1.2, {DATE})")
# U-1032: log/roadmap/S1/5_merge.md section 4
u = set_row(u, "U-1032",
            "no COMMAND segment presses it |",
            "no COMMAND segment presses it. Resolved: chareditor.cpp ports the bio over the EditBox family (create, setText, "
            "resizeBuffer, insertChar, onKey without Up / Down, resetFields, releaseState); keys the focus child takes come from tick.cpp "
            "parse_trace as RootHandler commands 12 / 13. Judged by port/test/test_chareditor_bio.py (emulator oracle on the original "
            "with the key routing for real: 7 x 300 EditBox cases and 120 editor cases with 212 bio entries GREEN; test_chareditor.py's "
            "oracle runs the bio too, 300 / 300) and by the pinned scenario cov_r_bio (boot gate PASS 687 / 687, 5 controls RED). "
            "Residual: U-1190..U-1195. |",
            "open", f"resolved (S1.5, {DATE})")
# U-1051: log/roadmap/S1/4_merge.md section 2
u = set_row(u, "U-1051",
            "during a world load in the corpus. |",
            "during a world load in the corpus. Resolved 2026-10-24 (S1.4): scenario cov_r_flic_input posts a `z` in "
            "courseActionMenu's waitTicks; the original dispatches it from loadFlic's pumpInput483bd0 (call log: rootWin_onKey, return "
            "address 0x0047c67e, inside the 15th loadFlic pump, no clock read between the wait and the load: the trace puts the MSG "
            "before the load's first palette QPC). The port's TraceInput takes it with PumpKind::Load (log/roadmap/S1/tick_combined.diff); "
            "boot 75/75 equal, RED without the patch and with tick.frame 8. Open follow-ups U-1181 (why the 15th pump), U-1183. |",
            "open", f"resolved (S1.4, {DATE})")
# U-1066: parts 1 (S1.4, 4_merge.md section 2) and 2 (S1.3, 3_merge.md) resolved, part 3 open with U-1170
u = set_row(u, "U-1066",
            "showTutorialText's pages 1..29 (every call in the corpus has g_tutorialPage 0 and returns after the camera reset; controls "
            "1..3 read GREEN, 4 RED)",
            "showTutorialText's pages 1..29 (resolved 2026-10-24, S1.4: cov_r_tutorial runs pages 1..29 and 30 from boot, 1,195 records "
            "equal, controls showTutorialText 1..4 RED (port/test/test_s1_4_scenarios.py); the page word and bit 15 of g_flags are "
            "harness writes because no instruction stores a nonzero page (U-1182))",
            "open", f"parts 1 and 2 resolved (S1.4, S1.3, {DATE}); part 3 open (U-1170)")
u = set_row(u, "U-1066",
            "with the original's frame-head images). |",
            "with the original's frame-head images). S1.3: showThemePackSelect's Cancel is judged by cov_r_theme_cancel (control 2 RED); "
            "palette hotspots 21 / 22 are drawn by showInfoCursorPanel only, so loadInterfaceArt control 3 stays GREEN in the boot run "
            "and the cuts are judged as state (U-1170). |",
            f"parts 1 and 2 resolved (S1.4, S1.3, {DATE}); part 3 open (U-1170)",
            f"parts 1 and 2 resolved (S1.4, S1.3, {DATE}); part 3 open (U-1170)")
wr("UNCERTAINTIES.md", u)
print(f"edited UNCERTAINTIES.md: {len(rows)} rows appended")

# ------------------------------------------------------------------------------------------------ CLAUDE.md: S1 then S8
m1 = (S1 / "MERGE.md").read_bytes().decode("utf-8").replace("\r\n", "\n")
p1 = m1.split("## CLAUDE.md paragraph\n\n", 1)[1].split("\n\n## Gate command lines", 1)[0].strip("\n")
assert p1.startswith("Roadmap S1, loose ends of phases 3 and 4") and "\n\n" not in p1
m8 = (S8 / "MERGE.md").read_bytes().decode("utf-8").replace("\r\n", "\n")
p8 = m8.split("## CLAUDE.md paragraph (port section, after the last phase paragraph)\n\n```\n", 1)[1].split("\n```", 1)[0].strip("\n")
assert p8.startswith("S8 video, skip path") and "\n\n" not in p8
c = rd("CLAUDE.md")
if "Roadmap S1, loose ends of phases 3 and 4" in c or "S8 video, skip path" in c:
    sys.exit("CLAUDE.md: paragraph already present")
anchor = "\r\nFunction bodies are not always contiguous"
assert c.count(anchor) == 1
before = c.split(anchor, 1)[0]
last = [l for l in before.split("\r\n") if l.startswith(("Phase ", "Roadmap "))][-1]
assert last.startswith("Phase 7 defect decisions"), f"CLAUDE.md: the entry before the anchor is {last[:40]!r}"
add = "\r\n" + p1.replace("\n", "\r\n") + "\r\n" + p8.replace("\n", "\r\n")
wr("CLAUDE.md", c.replace(anchor, add + anchor, 1))
print("edited CLAUDE.md: S1 and S8 paragraphs")
