"""re/tools/model_scan.py, model_inventory.py and model_check.py (phase-1 stream A) against tables already typed by
hand: the golfer record (re/analysis/data/golfer_record.md), the record tables of record_tables.md, known vtables and
string tables. The scan of the game logic (~25 s) and the inventory (~30 s) run once per session."""
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import model_check as mc  # noqa: E402
import model_inventory as mi  # noqa: E402
import model_scan as ms  # noqa: E402

pytestmark = pytest.mark.static

GOLFER = "cstruct:re/analysis/data/golfer_record.md#Golfer"


def rng(**kw):
    row = {"module": "golf_clean.exe", "size": "", "count": "", "stride": "", "sign": "", "const": "", "fields": "",
           "name": ""}
    row.update(kw)
    return mc.Range(row)


@pytest.fixture(scope="module")
def refs():
    return mc.scan_refs()


@pytest.fixture(scope="module")
def inv(tmp_path_factory):
    return mi.run(out=tmp_path_factory.mktemp("model"))


def errors(res):
    return [v for v in res if v["severity"] == "error"]


# ---------------------------------------------------------------- model_check
def test_golfer_struct_layout_matches_its_offset_comments():
    fields = mc.parse_cstruct("re/analysis/data/golfer_record.md", "Golfer")
    assert max(f.end for f in fields) == 0x100
    by = {f.name: f for f in fields}
    assert (by["flags"].off, by["holeStrokes"].off, by["holeStrokes"].n, by["skill"].off) == (0x10, 0x23, 19, 0xf0)


def test_golfer_record_known_good_has_no_errors(refs):
    res = mc.check_range(rng(va="0x005794b8", type="struct", count="156", stride="0x100", fields=GOLFER,
                             name="golfers"), refs, 0x100)
    assert errors(res) == []
    # the warnings are real observations, kept for the data-model streams: byte tests of the flag dword +0x10 and
    # movsx reads of the 16-bit +0x16 (declared uint16_t)
    assert any(v["violation"] == "width_narrow" and v["off"] == 0x10 for v in res)
    assert any(v["violation"] == "sign" and v["off"] == 0x16 for v in res)


def test_golfer_record_with_the_old_base_reports_errors(refs):
    # the 0x5794c0 convention of the old hook sources, typed with the same struct: every field lands 8 bytes off
    res = mc.check_range(rng(va="0x005794c0", type="struct", count="156", stride="0x100", fields=GOLFER,
                             name="golfers_c0"), refs, 0x100)
    assert len([v for v in errors(res) if v["violation"] == "width_cross"]) >= 20


def test_golfer_record_as_short_array_reports_errors(refs):
    res = mc.check_range(rng(va="0x005794b8", type="short", count="0x4e00", name="short"), refs, 0x100)
    assert len([v for v in errors(res) if v["violation"] == "width_cross"]) >= 1000


def test_const_write_and_bad_stride(refs):
    # g_golferStats (record_tables.md) declared const and with a wrong stride 0xc0
    res = mc.check_range(rng(va="0x0056ae70", type="int", count="0x5c0", const="1", name="stats_const"), refs, 0)
    assert any(v["violation"] == "const_write" for v in res)
    res = mc.check_range(rng(va="0x0056ae70", type="struct", count="32", stride="0xc0",
                             fields="0x0:int[48]", name="stats_c0"), refs, 0)
    assert any(v["violation"] == "stride" and "0xb8" in v["detail"] for v in res)


def test_loop_end_against_the_walker_count(refs):
    # g_walkers 64 x 0x4c (record_tables.md): loops end at `cmp eax, 0x586b62` (0x0042774c, matchUpdate) and
    # `cmp ebx / esi, 0x586b63`. With 64 records they stay inside; with 63 the last record lies past the end.
    def walkers(count):
        return mc.check_range(rng(va="0x00585850", type="struct", count=count, stride="0x4c", fields="0x0:int[19]",
                                  name="walkers"), refs, 0x100)
    assert not [v for v in walkers("64") if v["violation"] == "loop_end"]
    assert 0x0042774C in {v["ins"] for v in walkers("63") if v["violation"] == "loop_end"}


# ---------------------------------------------------------------- scanner
def test_scanner_recovers_the_golferstats_multiplier(refs):
    # (cls + 4 * prefs) * 23 * 8: `lea eax, [edx + ecx*4]; lea ecx, [eax + eax*2]; shl ecx, 3; sub ecx, eax;
    # mov ebx, [ecx*8 + 0x56ae7c]` at 0x0042bdae..0x0042bdb9
    r = next(r for r in refs if r.ins == 0x0042BDB9 and r.addr == 0x0056AE7C)
    assert (0xB8, "scale") in r.strides


# ---------------------------------------------------------------- inventory
def test_keep_rule_drops_the_flag_constant(inv):
    # `or ebx, 0x800000` (0x00422fdf) and `test dword [0x59e7b8], 0x800000` (0x004250a2): a bit, not an address
    assert inv.rows[0x00800000]["kept"] == 0


def test_validation_tuned_set_is_all_correct(inv):
    tuned = [v for v in inv.validation if v[9].startswith("tuned ")]
    assert len(tuned) == len(mi.VALIDATION)
    assert [v for v in tuned if not v[8]] == []


def test_validation_holdout_rate(inv):
    hold = [v for v in inv.validation if v[9].startswith("holdout ")]
    assert len(hold) >= 50
    assert sum(v[8] for v in hold) >= 0.85 * len(hold)


def test_every_cluster_has_one_stream(inv):
    assert {inv.owners[c["id"]][0] for c in inv.clusters} <= set("BCDEFX")
    assert len(inv.owners) == len(inv.clusters)


def test_known_tables_are_covered(inv):
    names = {mi.Inventory.covered(inv, c)[0] for c in inv.clusters if mi.Inventory.covered(inv, c)}
    assert names == {k[0] for k in mi.KNOWN}


def test_known_vtables(inv):
    rows = {r[0]: r for r in inv.vtable_rows}
    assert rows["0x004ba2fc"][2] >= 40            # rootWin: slots 29..39 are the root window input handlers
    assert rows["0x004bacf8"][2] >= 29            # Snd: Snd::flags is slot 28
    assert rows["0x004bb014"][3] == "NodeListC"   # installed by NodeListC::ctor / dtor
