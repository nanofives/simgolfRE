"""I1 (S6 merge): apply the shared-file edits of log/roadmap/S6/MERGE.md items 1-5 to a tree root (the mirror first,
then the shared tree). Every edit is checked: the anchor must occur exactly once and the edit must not be present yet.
Usage: ~/bin/py -3.12 log/roadmap/merges/S6-1/apply_shared.py <root> [--evidence | --evidence-only] [--uncertainties-only]"""
import pathlib
import sys

S6 = pathlib.Path(__file__).resolve().parents[2] / "S6"   # log/roadmap/S6 (log is shared through the junction)
root = pathlib.Path(sys.argv[1]).resolve()
evidence = "--evidence" in sys.argv[2:]
only_u = "--uncertainties-only" in sys.argv[2:]  # re-apply item 1 alone (used once, after the address-cell fix)
CRLF = "\r\n"


def rd(p):
    return (root / p).read_bytes().decode("utf-8")


def wr(p, s):
    (root / p).write_bytes(s.encode("utf-8"))
    print("edited", p)


def once(text, anchor, what):
    n = text.count(anchor)
    if n != 1:
        sys.exit(f"{what}: anchor occurs {n} times")


# gate edit (MERGE.md's Gate, last line; --evidence or --evidence-only): S6's state tests in the integrator ledger's
# evidence map, added after the defects run listed the six switches as `no evidence`
def apply_evidence():
    t = rd("port/test/integrator.py")
    ta = ('                 "narrateShot.maskNotRestored": "port/test/test_shots.py::test_defect_narrate_mask_not_restored (state)"}')
    once(t, ta, "integrator.py evidence map")
    add = ('                 "narrateShot.maskNotRestored": "port/test/test_shots.py::test_defect_narrate_mask_not_restored (state)",\r\n'
           '                 # S6 (log/roadmap/S6/MERGE.md): each judged by the state the original hangs or faults on (Unicorn probes\r\n'
           '                 # log/roadmap/S6/6_probes/) plus a completing control equal at 1 and 0 and a wrong body that reads RED\r\n'
           '                 "placeHabitat.clusterLoopBound": "port/test/test_defects_golfers.py::test_place_habitat_table_full_slot_127 (state)",\r\n'
           '                 "spawnObject.cellSearchBound": "port/test/test_defects_golfers.py::test_spawn_object_full_bank (state)",\r\n'
           '                 "loadGolfPro.divideAndRejectGuard": "port/test/test_defects_golfers.py::test_load_golf_pro_zero_divisor, "\r\n'
           '                                                     "test_load_golf_pro_empty_roster (state)",\r\n'
           '                 "placeAnimal.zeroFrameCount": "port/test/test_defects_golfers.py::test_place_animal_zero_frame_count (state)",\r\n'
           '                 "replaceInText.tailLimit": "port/test/test_defects_gen.py::test_d46_overflowing_tail (state)",\r\n'
           '                 "generateRandomCourse.searchBound": "port/test/test_defects_gen.py::test_d52b1_type_0x11_window_without_a_cell, "\r\n'
           '                                                     "test_d52b2_clubhouse_window_without_a_spot (state)"}')
    if "\r\n" not in t:
        add = add.replace("\r\n", "\n")
    wr("port/test/integrator.py", t.replace(ta, add, 1))


if "--evidence-only" in sys.argv[2:]:
    apply_evidence()
    sys.exit(0)

# 1. UNCERTAINTIES.md: the 28 rows of uncertain_rows.md, plus U-1590 (4_uncertain.tsv has no header line, so the
#    generator log/roadmap/S6/uncertain_rows.py took its only row for the header and dropped it; MERGE.md names the range
#    U-1580..U-1590 and its CLAUDE.md paragraph cites U-1590). Built in the generator's shape for a header-less row.
rows = [l for l in (S6 / "uncertain_rows.md").read_bytes().decode("utf-8").splitlines() if l.strip()]
u1590 = (S6 / "4_uncertain.tsv").read_bytes().decode("utf-8").splitlines()[0].split("\t")
assert u1590[0] == "U-1590" and len(u1590) == 3, u1590
rows.append(f"| {u1590[0]} |  | behavioural | [S6, 4] {u1590[2]} |  | {u1590[1]} |")
assert len(rows) == 29, len(rows)
# 5_uncertain.tsv names its description column `text`, which the generator does not read, so its rows (U-1595..U-1597)
# came out with an empty description: take it from the TSV (the generator's kind mapping: reach / choice -> behavioural).
src5 = [l.split("\t") for l in (S6 / "5_uncertain.tsv").read_bytes().decode("utf-8").splitlines()]
assert src5[0] == ["id", "address", "kind", "text", "status"], src5[0]
by5 = {r[0]: r for r in src5[1:] if r and r[0].startswith("U-")}
for i, r in enumerate(rows):
    uid = r.split(" | ")[0].strip("| ")
    if uid in by5:
        d = dict(zip(src5[0], by5[uid]))
        assert f"| {uid} | {d['address']} | behavioural | [S6, 5]  |  | {d['status']} |" == r, r
        rows[i] = f"| {uid} | {d['address']} | behavioural | [S6, 5] {d['text']} |  | {d['status']} |"
# scripts/re_classify.py (open_blocking_uncertainties) reads the address cell of every row as hex, and 6_uncertain.tsv
# puts a function name there for U-1595..U-1597 (test_classify.py::test_real_tracker_state raised ValueError): such a
# cell is moved to the front of the description and the address cell left empty, as other function-level rows have it.
fixed = []
for r in rows:
    c = r.split(" | ")
    a = c[1].strip()
    try:
        int(a or "0", 16)
    except ValueError:
        c[1], c[3] = "", c[3].replace("] ", f"] {a}: ", 1).rstrip()
        print("address cell moved into the description:", c[0].strip("| "), repr(a))
    fixed.append(" | ".join(c))
rows = fixed
u = rd("UNCERTAINTIES.md")
if "| U-1550 |" in u:
    sys.exit("UNCERTAINTIES.md: S6 rows already present")
assert u.endswith(CRLF)
wr("UNCERTAINTIES.md", u + CRLF.join(rows) + CRLF)
if only_u:
    sys.exit(0)

# 3. re/PORT_PLAN.md status table, row 7
old = "not started; every reproduced defect has a switch (default = the original)"
new = ("decisions listed (re/port/defect_decisions.tsv, 74 rows: 10 fix, 26 ask Mariano, 38 keep); profile fixed = 6 switches "
       "over the hang and fault defects (re/port/defect_profiles.tsv); profile original = every switch 1")
p = rd("re/PORT_PLAN.md")
once(p, "| 7. Defect decisions | " + old + " |", "PORT_PLAN row 7")
wr("re/PORT_PLAN.md", p.replace(old, new))

# 4. CLAUDE.md: the paragraph after the last port-history entry (Phase 5 step 2), before "Function bodies are not ..."
m = (S6 / "MERGE.md").read_bytes().decode("utf-8").replace("\r\n", "\n")
para = m.split("## CLAUDE.md paragraph\n\n", 1)[1].split("\n\n## Gate", 1)[0].strip("\n")
assert para.startswith("Phase 7 defect decisions") and "\n\n" not in para
c = rd("CLAUDE.md")
if "Phase 7 defect decisions, section S6" in c:
    sys.exit("CLAUDE.md: paragraph already present")
anchor = CRLF + "Function bodies are not always contiguous"
once(c, anchor, "CLAUDE.md anchor")
before = c.split(anchor, 1)[0]
last = [l for l in before.split(CRLF) if l.startswith("Phase ")][-1]
assert last.startswith("Phase 5 step 2,"), f"CLAUDE.md: the entry before the anchor is {last[:40]!r}"
wr("CLAUDE.md", c.replace(anchor, CRLF + para.replace("\n", CRLF) + anchor, 1))

# 5. optional C ABI (log/roadmap/S6/3_merge.md): sg_profile_set / sg_profile_get
h = rd("port/include/portsim_c.h")
ha = "SG_API void sg_defect_reset(void);                      /* every switch back to 1 */\r\n"
once(h, ha, "portsim_c.h anchor")
hd = ("/* Profiles over the sg_defect switches (S6, log/roadmap/S6/3_merge.md). `original` = every switch 1. `fixed` = the members\r\n"
      " * the caller passes (switch names and values, from re/port/defect_profiles.tsv read by the host): every switch is reset to 1,\r\n"
      " * then each member is set. Returns SG_OK, or SG_E_ARG for an unknown profile or switch, a NULL list with n > 0, n < 0, or a\r\n"
      " * member list given with `original` (nothing is changed then). */\r\n"
      "SG_API int sg_profile_set(const char *profile, const char *const *switches, const int32_t *values, int32_t n);\r\n"
      "/* \"original\" when every switch is 1, \"fixed\" when set by sg_profile_set(\"fixed\", ...) and not changed since, else \"custom\". */\r\n"
      "SG_API const char *sg_profile_get(void);\r\n")
wr("port/include/portsim_c.h", h.replace(ha, ha + hd, 1))

s = rd("port/src/portsim_c.cpp")
sa_set = ("int sg_defect_set(const char *name, int32_t on) {\r\n"
          "  const portsim::DefectDef *d = portsim::defect_at(portsim::defect_find(name), nullptr);\r\n"
          "  if (!d) return SG_E_ARG;\r\n")
once(s, sa_set, "portsim_c.cpp sg_defect_set")
s = s.replace(sa_set, sa_set + "  g_profile_fixed = false;\r\n", 1)
sa_def = "// ------------------------------------------------------------------------------------------------ defects (phase 3.0)\r\n"
once(s, sa_def, "portsim_c.cpp defects banner")
s = s.replace(sa_def, sa_def + "namespace {\r\nbool g_profile_fixed = false;  // the last sg_profile_set was \"fixed\" and no switch was set directly since\r\n}  // namespace\r\n\r\n", 1)
sa_reset = ("void sg_defect_reset(void) {\r\n"
            "  for (int i = 0; i < portsim::defect_count(); ++i) *portsim::defect_at(i, nullptr)->flag = 1;\r\n"
            "}\r\n")
once(s, sa_reset, "portsim_c.cpp sg_defect_reset")
prof = ("\r\n"
        "// S6: profiles over the switches (log/roadmap/S6/3_merge.md); every name is checked before anything changes\r\n"
        "int sg_profile_set(const char *profile, const char *const *switches, const int32_t *values, int32_t n) {\r\n"
        "  if (!profile || n < 0 || (n > 0 && (!switches || !values))) return SG_E_ARG;\r\n"
        "  const bool fixed = std::strcmp(profile, \"fixed\") == 0;\r\n"
        "  if (!fixed && (std::strcmp(profile, \"original\") != 0 || n != 0)) return SG_E_ARG;\r\n"
        "  for (int32_t i = 0; i < n; ++i)\r\n"
        "    if (!switches[i] || !portsim::defect_at(portsim::defect_find(switches[i]), nullptr)) return SG_E_ARG;\r\n"
        "  sg_defect_reset();\r\n"
        "  for (int32_t i = 0; i < n; ++i)\r\n"
        "    *portsim::defect_at(portsim::defect_find(switches[i]), nullptr)->flag = values[i] ? 1 : 0;\r\n"
        "  g_profile_fixed = fixed;\r\n"
        "  return SG_OK;\r\n"
        "}\r\n"
        "\r\n"
        "const char *sg_profile_get(void) {\r\n"
        "  bool all_on = true;\r\n"
        "  for (int i = 0; i < portsim::defect_count(); ++i)\r\n"
        "    if (*portsim::defect_at(i, nullptr)->flag != 1) all_on = false;\r\n"
        "  if (all_on) return \"original\";\r\n"
        "  return g_profile_fixed ? \"fixed\" : \"custom\";\r\n"
        "}\r\n")
wr("port/src/portsim_c.cpp", s.replace(sa_reset, sa_reset + prof, 1))
if evidence:
    apply_evidence()
