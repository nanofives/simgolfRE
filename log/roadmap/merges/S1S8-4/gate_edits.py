"""I4 gate edits (beyond the two MERGE.md texts, each needed for a gate). An edit already present is skipped; an anchor missing
otherwise stops the script.
1-2. S8 item 5 widens re/tools/scenario.py's hook glob to scenario_s[18]_*.py, but S8.2's own test still asserted the old glob
     (test_scenario_hook_in_scenario_py_does_not_load_this_module failed in the mirror's suite) and scenario_s8_2.py's docstring
     said the hook does not pick it up. Both now describe the widened hook.
3.   S8 item 4 (optional, applied) makes the frameless boot branch report "mainLoop returned (the title menu's quit)" for S8.2's
     title-menu-only trace; MERGE.md says test_video.py accepts both texts, but test_boot_s8_2_full_trace asserted only the old
     one and failed. It now asserts the item-4 text.
Usage: ~/bin/py -3.12 log/roadmap/merges/S1S8-4/gate_edits.py <root>"""
import pathlib
import sys

root = pathlib.Path(sys.argv[1]).resolve()


def edit(p, old, new, what):
    t = (root / p).read_bytes().decode("utf-8")
    for o, n in ((old, new), (old.replace("\n", "\r\n"), new.replace("\n", "\r\n"))):
        if t.count(o) == 1:
            (root / p).write_bytes(t.replace(o, n, 1).encode("utf-8"))
            print(f"edited {p}: {what}")
            return
    for n in (new, new.replace("\n", "\r\n")):
        if t.count(n) == 1:
            print(f"already applied {p}: {what}")
            return
    sys.exit(f"{p} ({what}): anchor not found once")


edit("port/test/test_s8_2_scenarios.py",
     "def test_scenario_hook_in_scenario_py_does_not_load_this_module():\n"
     "    \"\"\"re/tools/scenario.py's end-of-file hook globs scenario_s1_*.py only; the driver imports scenario_s8_2 itself.\"\"\"\n"
     "    src = (ROOT / \"re\" / \"tools\" / \"scenario.py\").read_text(encoding=\"utf-8\")\n"
     "    assert 'scenario_s1_*.py' in src and 'scenario_s8' not in src\n",
     "def test_scenario_hook_in_scenario_py_loads_this_module():\n"
     "    \"\"\"re/tools/scenario.py's end-of-file hook globs scenario_s[18]_*.py (S8 merge item 5), so it registers this module; the\n"
     "    driver still imports scenario_s8_2 itself.\"\"\"\n"
     "    src = (ROOT / \"re\" / \"tools\" / \"scenario.py\").read_text(encoding=\"utf-8\")\n"
     "    assert 'scenario_s[18]_*.py' in src\n",
     "hook test follows the widened glob")
edit("re/tools/scenario_s8_2.py",
     "\"\"\"Roadmap S8.2 scenarios (log/roadmap/S8/2_notes.md). NOT picked up by re/tools/scenario.py's end-of-file hook (it globs\n"
     "scenario_s1_*.py only): re/tools/s8_movie_runs.py imports this module explicitly before scenario_refs.setup runs.\n",
     "\"\"\"Roadmap S8.2 scenarios (log/roadmap/S8/2_notes.md). Registered by re/tools/scenario.py's end-of-file hook (it globs\n"
     "scenario_s[18]_*.py); re/tools/s8_movie_runs.py also imports this module explicitly before scenario_refs.setup runs.\n",
     "docstring follows the widened glob")
edit("port/test/test_video.py",
     "    # tick.cpp's frameless boot branch reports the end of the trace whatever the boot step returned (3_merge.md item 4 makes it\n"
     "    # say \"mainLoop returned\"); every recorded dispatch is taken, the closing movie (QUIT only) reached the host and top10.sve\n"
     "    # (QUIT, before the movie) was written\n"
     "    if detail:         # the Win32 runner reports no run detail\n"
     "        assert \"ran to the end of the trace\" in detail and \" 0 left\" in detail, detail[:300]\n",
     "    # tick.cpp's frameless boot branch keeps the boot step's FrameExit and reports the quit (3_merge.md item 4, merged by I4);\n"
     "    # every recorded dispatch is taken, the closing movie (QUIT only) reached the host and top10.sve (QUIT, before the movie)\n"
     "    # was written\n"
     "    if detail:         # the Win32 runner reports no run detail\n"
     "        assert \"mainLoop returned (the title menu's quit)\" in detail and \" 0 left\" in detail, detail[:300]\n",
     "frameless boot detail: the item-4 text")
