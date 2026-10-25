"""I5: the gate edits the merge needed beyond the merge requests' texts (anchored, refuse to apply twice).

1. port/test/test_s8_2_scenarios.py::test_scenario_hook_in_scenario_py_loads_this_module asserted S8's hook glob literally
   (`scenario_s[18]_*.py`); S2 wave 4 item 1.1 widens it to `scenario_s[0-9]*_*.py`, so the test now asserts the widened glob and that
   it matches this module's file name (fnmatch), which is what the test is for. re/tools/scenario_s8_2.py's docstring names the glob.

  ~/bin/py -3.12 log/roadmap/merges/S2S3-5/gate_edits.py <root>"""
import pathlib
import sys

root = pathlib.Path(sys.argv[1])


def edit(rel, old, new, what):
    p = root / rel
    t = p.read_bytes().decode("utf-8")
    nl = "\r\n" if "\r\n" in t else "\n"
    o, n = old.replace("\n", nl), new.replace("\n", nl)
    if n in t and o not in t:
        sys.exit(f"{what}: already applied")
    if t.count(o) != 1:
        sys.exit(f"{what}: anchor found {t.count(o)} times")
    p.write_bytes(t.replace(o, n).encode("utf-8"))
    print("edit", rel, what)


edit("port/test/test_s8_2_scenarios.py",
     '''    """re/tools/scenario.py's end-of-file hook globs scenario_s[18]_*.py (S8 merge item 5), so it registers this module; the
    driver still imports scenario_s8_2 itself."""
    src = (ROOT / "re" / "tools" / "scenario.py").read_text(encoding="utf-8")
    assert 'scenario_s[18]_*.py' in src
''',
     '''    """re/tools/scenario.py's end-of-file hook globs scenario_s[0-9]*_*.py (S8 merge item 5, widened by S2 wave 4), so it registers
    this module; the driver still imports scenario_s8_2 itself."""
    import fnmatch
    src = (ROOT / "re" / "tools" / "scenario.py").read_text(encoding="utf-8")
    assert 'scenario_s[0-9]*_*.py' in src
    assert fnmatch.fnmatch("scenario_s8_2.py", "scenario_s[0-9]*_*.py")
''', "1 S8 hook test follows the widened glob")
edit("re/tools/scenario_s8_2.py",
     "scenario_s[18]_*.py); re/tools/s8_movie_runs.py",
     "scenario_s[0-9]*_*.py); re/tools/s8_movie_runs.py", "1 scenario_s8_2 docstring")
