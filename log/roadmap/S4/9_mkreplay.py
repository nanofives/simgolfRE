"""One-shot generator of log/roadmap/S4/9_replay.py from 8_replay.py (S4.8's front end): S4.9 mirror, one golden file, the S4.9 rows,
the fallback-owner gate of 5_run.py (a fallback to a function an ACCEPTED step owns fails the key). Re-running regenerates the file;
edit 9_replay.py's DERIVED9 / CUSTOM9 sections by hand afterwards only through this script's SECTION text below."""
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
t = (HERE / "8_replay.py").read_text(encoding="utf-8")


def sub(old, new, count=1):
    global t
    assert t.count(old) >= 1, old[:60]
    t = t.replace(old, new) if count == 0 else t.replace(old, new, count)


sub('"""S4.8\'s replay front end over', '"""S4.9\'s replay front end (a copy of 8_replay.py: golden file, mirror, rows and the fallback-owner gate are S4.9\'s) over')
sub("8_replay.py", "9_replay.py", 0)
sub("The S4.8 mirror", "The S4.9 mirror")
sub("S4_8_mirror", "S4_9_mirror", 0)
sub('GOLDENS = {"ds": ROOT / "port" / "test" / "golden_audio_ds.json", "dev": ROOT / "port" / "test" / "golden_audio_dev.json"}',
    'GOLDENS = {"wave": ROOT / "port" / "test" / "golden_audio_wave.json"}')
sub('("8_fixtures.js", "sound_replay_s48_fixtures.js")', '("9_fixtures.js",)')
sub('HERE / "8_fixtures.js")', 'HERE / "9_fixtures.js")')
sub('"8_fixtures.js: "', '"9_fixtures.js: "')
sub('R.step_rows("S4.8")', 'R.step_rows("S4.9")')
sub('if r["family"].rstrip("~") == family]', 'if r["family"].rstrip("~") == "wave"]')
sub('choices=["ds", "dev"]', 'choices=["wave"]')
sub('r = self.vm.request({"op": "eval", "src": FIXTURES_JS.read_text()})',
    'r = self.vm.request({"op": "eval", "src": (HERE / "8_fixtures.js").read_text() + "\\n" + FIXTURES_JS.read_text()})')
# the emu-only sections: S4.9's, loaded from 9_emuonly.py when it exists
a = t.index("# emu-only rows of S4.8")
b = t.index("# registry keys the gate cannot run")
c = t.index("def family_keys")
t = (t[:a] + "DERIVED8 = {}\nCUSTOM8 = {}\nEXCLUDED8 = {}\n"
     "try:\n    sys.path.insert(0, str(HERE))\n    from emuonly9 import DERIVED8, CUSTOM8, EXCLUDED8  # noqa: E402,F811\nexcept ImportError:\n    pass\n\n\n" + t[c:])
# fallback owners: layout A records of every key, by the step that owns the callee
sub("def oracle(keys, layouts, update, out, family):", '''OWN_STEPS = ("S4.2", "S4.4", "S4.5", "S4.6", "S4.8", "S4.9")     # ACCEPTED or this step: a fallback to one of their functions fails
_OWNERS = None


def fallback_owners(recs):
    """layout A's fallback calls of a key: {owner step: calls} from each vector's report (rvas, decimal, comma separated)"""
    global _OWNERS
    if _OWNERS is None:
        _OWNERS = {int(r["rva"], 16): r["step"] for r in R.step_rows_all()}
    c = {}
    for r in recs or []:
        for x in (r.report.get("fallback_rvas") or "").split(","):
            if x:
                owner = _OWNERS.get(int(x), "unlisted")
                c[owner] = c.get(owner, 0) + 1
    return c


FB_RVAS = {}


def oracle(keys, layouts, update, out, family):''')
sub("v, recs, regions = R.oracle_key(vm, lib, key, tuple(layouts), record=update)",
    "v, recs, regions = R.oracle_key(vm, lib, key, tuple(layouts), record=True)\n                fb = fallback_owners(recs)\n                own_fb = sum(n for s, n in fb.items() if s in OWN_STEPS)\n                for r_ in recs or []:\n                    for x in (r_.report.get('fallback_rvas') or '').split(','):\n                        if x:\n                            FB_RVAS[int(x)] = FB_RVAS.get(int(x), 0) + 1")
sub("""ok = (v.match == v.vectors and v.match_bc == v.vectors * (len(layouts) - 1) and v.fallbacks == 0
                      and v.tier in ("EXACT", "EQUAL_PI", "EQUAL_PI_DISTINCT_DIFF", "EMU_ONLY"))""",
    """ok = (v.match == v.vectors and v.match_bc == v.vectors * (len(layouts) - 1) and v.fallbacks == 0
                      and v.tier in ("EXACT", "EQUAL_PI", "EQUAL_PI_DISTINCT_DIFF", "EMU_ONLY"))
                okown = (v.match == v.vectors and v.match_bc == v.vectors * (len(layouts) - 1) and own_fb == 0
                         and v.tier in ("EXACT", "EQUAL_PI", "EQUAL_PI_DISTINCT_DIFF", "EMU_ONLY"))""")
sub("""verdict = "GREEN" if ok else ("GREEN_FB" if okf else "RED")""", """verdict = "GREEN" if ok else ("GREEN_FB" if okown else "RED")""")
sub('f"fallback {v.fallbacks} tier {v.tier}', 'f"fallback {v.fallbacks} {fb} tier {v.tier}')
sub("if update and recs is not None and okf:", "if update and recs is not None and okown:")
sub('''"regions": regions, "fallbacks": v.fallbacks}''', '''"regions": regions, "fallbacks": v.fallbacks, "fallback_owners": fb}''')
sub('''    print(f"keys {len(rows)}  green {len(rows) - len(bad)}  red {len(bad)}  fallbacks {fb}  {' '.join(bad)}")''',
    '''    print(f"keys {len(rows)}  green {len(rows) - len(bad)}  red {len(bad)}  fallbacks {fb}  {' '.join(bad)}")
    print("fallback callees: " + ", ".join(f"0x{r:x}x{n}" for r, n in sorted(FB_RVAS.items())))''')
sub("v, recs, regions = R.oracle_key(vm, lib, key, tuple(layouts), record=True)",
    "v, recs, regions, forgiven = Q.oracle_key_q(R, vm, lib, key, tuple(layouts), True, QUIRK_KEYS)")
sub("OWN_STEPS = (", """import quirk9 as Q  # noqa: E402
try:
    from emuonly9 import QUIRK_KEYS  # noqa: E402
except ImportError:
    QUIRK_KEYS = ()
OWN_STEPS = (""")
sub('"regions": regions, "fallbacks": v.fallbacks, "fallback_owners": fb}', '''"regions": regions, "fallbacks": v.fallbacks, "fallback_owners": fb,
                                   "quirk": {str(i): n for i, n in forgiven.items()}}''')
sub('f"fallback {v.fallbacks} {fb} tier {v.tier}', 'f"{("quirk-forgiven " + str(forgiven) + " ") if forgiven else ""}fallback {v.fallbacks} {fb} tier {v.tier}')
# the stand-ins of S4.9's keys record a SUBSET of an import's slots (the pointer slots dropped) or a transform of them (mmio stand-in:
# mmioOpenA records crc32(name), has-info, flags; mmioDescend records handle, has-parent, flags; c3dh's memory-open records name, flags
# and the 18 MMIOINFO dwords): the command's slots must be reachable from the record by those rules
a = t.index("def _records_match(args, vals) -> bool:")
b = t.index("def sinklog(")
t = t[:a] + '''def _subsequence(vals, args) -> bool:
    it = iter(args)
    return all(any(v == x for x in it) for v in vals)


def _records_match(args, vals, nm="") -> bivalent:
    """A stand-in records all slots, the N leading ones (`record: N`), an index list (pointer slots dropped: the values are a subsequence
    of the slots) or, for the mmio imports, the transforms of mmiostandin.js (and c3dh's memory-open record)."""
    n = len(vals)
    if args[:n] == vals or (n > 0 and args[len(args) - n:] == vals) or _subsequence(vals, args):
        return True
    if nm.endswith("mmioOpenA") and len(args) == 3:
        if n == 3 and vals[2] == args[2] and vals[1] == (1 if args[1] else 0):
            return True            # mmiostandin.js: crc32(name) or 0, info != NULL, flags
        if n == 20 and (vals[0], vals[1]) == (args[0], args[2]):
            return True            # c3dh's memory-open stand-in: name, flags, then the 18 MMIOINFO dwords
    if nm.endswith("mmioDescend") and len(args) == 4:
        return n == 3 and vals[0] == args[0] and vals[1] == (1 if args[2] else 0) and vals[2] == args[3]
    if nm.endswith("mmioAdvance") and len(args) == 3:
        return n == 3 and vals[0] == args[0] and vals[1] == (1 if args[1] else 0) and vals[2] == args[2]
    if nm.endswith("mmioStringToFOURCCA") and len(args) == 2:
        return n == 2 and vals[1] == args[1]     # the stand-in records the four-character code of the string (a pointer here), then the flags
    return False


'''.replace("bivalent", "bool") + t[b:]
sub('_records_match(e["args"], vals)', '_records_match(e["args"], vals, nm)')
# a full log (0x400 bytes) drops later calls and counts them in its header: the lost counts must be EQUAL in both arms, and a command that
# appended no record is fine when the lost counter moved
sub('orig["lost"] == lost == 0', 'orig["lost"] == lost')
sub("before = len(decode_implog(bytes(cur))[0])", "dec0 = decode_implog(bytes(cur))\n                before, lost_b = len(dec0[0]), dec0[1]")
sub("new = decode_implog(bytes(cur))[0][before:]", "dec1 = decode_implog(bytes(cur))\n                new, lost_a = dec1[0][before:], dec1[1]")
sub("                if len(new) != 1:\n", "                if len(new) == 0 and lost_a > lost_b:\n                    continue            # the log is full: the call is counted in the header, not recorded\n                if len(new) != 1:\n")
# import-level controls (mode 2 of a function changes the import calls it makes; the mode 1 controls change a result only):
# SG_SINK_NEGCTL=<function>:<mode> makes sinklog run the replay with that mode on
sub("""    R.GOLDEN = GOLDENS[family]
    lib = R.load_lib()
    golden = R.load_golden()
    imps = sound_imports()""",
    """    R.GOLDEN = GOLDENS[family]
    lib = R.load_lib()
    if os.environ.get('SG_SINK_NEGCTL'):
        _fn, _md = os.environ['SG_SINK_NEGCTL'].rsplit(':', 1)
        lib.negctl(_fn, int(_md))
    golden = R.load_golden()
    imps = sound_imports()""")
(HERE / "9_replay.py").write_text(t, encoding="utf-8")
print("wrote 9_replay.py", len(t.splitlines()), "lines")
