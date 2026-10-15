"""Three-axis scoreboard (scripts/scoreboard.py) on throwaway trees."""
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "scripts"))
import scoreboard as sb  # noqa: E402

pytestmark = pytest.mark.static


@pytest.fixture
def tree(tmp_path):
    (tmp_path / "re" / "coverage").mkdir(parents=True)
    (tmp_path / "log" / "diff").mkdir(parents=True)
    (tmp_path / "re" / "functions_ghidra.tsv").write_text(
        "entry\tname\tsize\tcallers\tcallees\n"
        "00401000\tFUN_00401000\t100\t1\t0\n"   # live, untouched
        "00402000\tFUN_00402000\t100\t1\t0\n"   # dead, but understood + verified
        "00403000\tFUN_00403000\t100\t1\t0\n")  # live, understood + replay-verified
    (tmp_path / "re" / "coverage" / "sandbox_basic.tsv").write_text(
        "addr\tphases\truns_seen\n00401000\tboot,menu,sandbox\t1/1\n00403000\tsandbox\t1/1\n")
    (tmp_path / "hooks.csv").write_text(
        "addr,module,name,subsystem,confidence,status,note,file,callers,callees,frida_diff,scenario,notes\n"
        "00402000,golf_clean.exe,Dead,util,C2,,,,,,,,\n00403000,golf_clean.exe,Live,util,C3,,,,,,,,\n")
    (tmp_path / "log" / "diff" / "00402000_Dead.path1.csv").write_text("vector,o,r,m\n1,1,1,True\nVERDICT,GREEN,1,\n")
    rows = "\n".join(f"{i},{i},{i},True" for i in range(150))
    (tmp_path / "log" / "diff" / "00403000_Live.replay.csv").write_text(f"vector,o,r,m\n{rows}\nVERDICT,GREEN,150,\n")
    return tmp_path


def by_addr(rows):
    return {r["addr"]: r for r in rows}


def test_axes(tree):
    r = by_addr(sb.build(tree))
    assert (r["00401000"]["reach"], r["00401000"]["understanding"], r["00401000"]["verification"]) == (3, 0, 0)
    assert (r["00402000"]["reach"], r["00402000"]["understanding"], r["00402000"]["verification"]) == (0, 2, 1)
    assert (r["00403000"]["reach"], r["00403000"]["understanding"], r["00403000"]["verification"]) == (1, 3, 2)


def test_dead_code_never_outranks_live_code(tree):
    rows = sb.build(tree)
    assert rows[0]["addr"] == "00401000"
    assert by_addr(rows)["00402000"]["priority"] == 0


def test_short_replay_counts_as_path1_only(tree):
    rows = "\n".join(f"{i},{i},{i},True" for i in range(20))
    (tree / "log" / "diff" / "00403000_Live.replay.csv").write_text(f"vector,o,r,m\n{rows}\nVERDICT,GREEN,20,\n")
    assert by_addr(sb.build(tree))["00403000"]["verification"] == 1


def test_red_evidence_counts_for_nothing(tree):
    (tree / "log" / "diff" / "00402000_Dead.path1.csv").write_text("vector,o,r,m\n1,1,2,False\nVERDICT,RED,1,\n")
    assert by_addr(sb.build(tree))["00402000"]["verification"] == 0


def test_a_red_key_cancels_its_kind(tree):
    # like the gate: one RED path-1 key among GREEN ones means the path-1 evidence does not count
    (tree / "log" / "diff" / "00402000_Dead_b.path1.csv").write_text("vector,o,r,m\n1,1,2,False\nVERDICT,RED,1,\n")
    assert by_addr(sb.build(tree))["00402000"]["verification"] == 0


def test_replay_calls_sum_over_files(tree):
    rows = "\n".join(f"{i},{i},{i},True" for i in range(60))
    for k in ("a", "b"):
        (tree / "log" / "diff" / f"00403000_Live_{k}.replay.csv").write_text(f"vector,o,r,m\n{rows}\nVERDICT,GREEN,60,\n")
    (tree / "log" / "diff" / "00403000_Live.replay.csv").unlink()
    assert by_addr(sb.build(tree))["00403000"]["verification"] == 2      # 60 + 60 >= REPLAY_MIN


def test_dll_evidence_counts_only_for_its_module(tree):
    # the three DLLs share base 0x10000000: a jgld CSV at a Terrain function's RVA is not that function's evidence
    (tree / "re" / "functions_ghidra_Terrain.dll.tsv").write_text("entry\tname\tsize\tcallers\tcallees\n00001d50\tFUN_10001d50\t50\t1\t0\n")
    d = tree / "log" / "diff"
    (d / "00001d50_jgld_Other.match.csv").write_text("x\nVERDICT,GREEN,1,\n")
    (d / "00001d50_TerrainDll_untagged.match.csv").write_text("x\nVERDICT,GREEN,1,\n")   # no module stem: not attributed
    terrain = [r for r in sb.build(tree) if r["module"] == "Terrain.dll"][0]
    assert terrain["verification"] == 0
    (d / "00001d50_Terrain_tileAt.path1.csv").write_text("vector,o,r,m\n1,1,1,True\nVERDICT,GREEN,1,\n")
    terrain = [r for r in sb.build(tree) if r["module"] == "Terrain.dll"][0]
    assert terrain["verification"] == 1 and terrain["evidence"].endswith("00001d50_Terrain_tileAt.path1.csv")


def test_unknown_reach_without_coverage(tree):
    (tree / "re" / "coverage" / "sandbox_basic.tsv").unlink()
    assert {r["reach"] for r in sb.build(tree)} == {"?"}
