"""Phase 2.5 function-vector corpus: schema, reader / writer, address map and emulator (static, no game).

Synthetic vectors only; the real corpus is local (port/corpus/vectors, gitignored). The emulator cases need the anchored
original\\golf_clean.exe (skipped without it). The end-to-end capture of a real function is tests/test_vector_capture_runtime.py.
"""
import copy
import gzip
import json
import re
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = pytest.mark.static
EXE = ROOT / "original" / "golf_clean.exe"
need_exe = pytest.mark.skipif(not EXE.exists(), reason="original\\golf_clean.exe missing")


@pytest.fixture(scope="module")
def gs():
    return VL.GsMap()


def test_gsmap_covers_the_data_section(gs):
    assert gs.items[0][0] == 0
    assert sum(i[1] for i in gs.items) == VL.DATA_SIZE
    for sym in ("g_rng", "g_date"):
        off, size = gs.by_sym[sym][0], gs.by_sym[sym][1]
        assert gs.ref(VL.DATA_VA + off) == (sym, 0)
        assert gs.ref(VL.DATA_VA + off + size - 1) == (sym, size - 1)
        assert gs.va(sym, 2) == VL.DATA_VA + off + 2
    with pytest.raises(ValueError):
        gs.ref(VL.DATA_VA - 1)
    with pytest.raises(ValueError):
        gs.ref(VL.DATA_VA + VL.DATA_SIZE)


def test_split_cuts_at_member_boundaries(gs):
    off, size, sym = gs.by_sym["g_rng"][:3]
    va = VL.DATA_VA + off - 2
    regs = gs.split(va, bytes(range(size + 4)))
    assert sum(len(r["hex"]) // 2 for r in regs) == size + 4
    assert [r["sym"] for r in regs][1] == "g_rng" and regs[1]["off"] == 0 and len(regs[1]["hex"]) // 2 == size
    for r in regs:   # every region lies inside one member or gap
        assert r["off"] + len(r["hex"]) // 2 <= gs.by_sym[r["sym"]][1]


def test_runs():
    assert VL.runs([5, 3, 4, 9, 4]) == [(3, 3), (9, 1)]
    assert VL.runs([]) == []


@pytest.mark.parametrize("t, eax, edx, want", [
    ("int", 0xFFFFFFFF, 0, -1), ("uint32", 0xFFFFFFFF, 0, 0xFFFFFFFF), ("uint8", 0x1234, 0, 0x34),
    ("int8", 0x80, 0, -128), ("uint16", 0x12345678, 0, 0x5678), ("bool", 0x100, 0, 0),
    ("int64", 0xFFFFFFFF, 0xFFFFFFFF, -1), ("uint64", 1, 2, (2 << 32) | 1), ("void", 7, 0, None), ("pointer", 0x4c1000, 0, 0x4c1000),
])
def test_norm_ret(t, eax, edx, want):
    assert VL.norm_ret(t, eax, edx) == want


def test_f80_to_float():
    assert VL.f80_to_float(1 << 63, 16383) == 1.0
    assert VL.f80_to_float(0xA000000000000000, 0x8000 | 16384) == -2.5
    assert VL.f80_to_float(0, 0) == 0.0
    assert VL.f80_to_float(1 << 63, 0x7FFF) == float("inf")
    v = VL.f80_to_float(0xCCCCCCCCCCCCCCCD, 16379)   # 0.1 in extended precision, rounded to double
    assert v == 0.1
    assert VL.float_eq(float("nan"), float("nan")) and not VL.float_eq(0.0, -0.0)


def synthetic_doc():
    return {"schema": VL.SCHEMA, "function": "synthetic", "module": "golf_clean.exe", "tooling_va": "0x00467170",
            "keys": [{"key": "k", "abi": "default", "ret": "int", "args": ["int", "pointer"], "fpcw": 0x27F,
                      "blocks": [{"id": 0, "role": "fixture", "size": 4, "hex": "01020304"}],
                      "relocs": [{"at": {"block": 0, "off": 0}, "to": {"block": 0, "off": 2}}],
                      "seed": [{"sym": "g_rng", "off": 0, "hex": "0100"}], "const": [],
                      "vectors": [{"i": 0, "status": "ok", "args": [{"int": 3}, {"ptr": {"block": 0, "off": 1}}],
                                   "ret": 5, "in": [], "out": [{"sym": "g_date", "off": 0, "hex": "ff"}]}]}]}


def test_vec_roundtrip_is_deterministic(tmp_path):
    d = synthetic_doc()
    a, b = tmp_path / "a.vec", tmp_path / "b.vec"
    VL.write_vec(a, d)
    VL.write_vec(b, copy.deepcopy(d))
    assert a.read_bytes() == b.read_bytes()            # gzip mtime 0: same content, same file
    assert VL.read_vec(a) == d
    with gzip.open(a) as f:
        assert json.loads(f.read())["schema"] == VL.SCHEMA


@pytest.mark.parametrize("mutate", [
    lambda d: d.update(schema="simgolf-vec/0"),
    lambda d: d["keys"][0].pop("fpcw"),
    lambda d: d["keys"][0].update(ret="long double"),
    lambda d: d["keys"][0]["blocks"][0].update(size=5),
    lambda d: d["keys"][0]["relocs"][0]["to"].update(block=3),
    lambda d: d["keys"][0]["vectors"][0]["args"].pop(),
    lambda d: d["keys"][0]["vectors"][0]["args"][1]["ptr"].update(sym="g_rng"),
    lambda d: d["keys"][0]["vectors"][0].pop("out"),
    lambda d: d["keys"][0]["seed"][0].pop("hex"),
])
def test_validate_rejects_malformed(mutate, tmp_path):
    d = synthetic_doc()
    mutate(d)
    with pytest.raises(VL.SchemaError):
        VL.validate_doc(d)
    with pytest.raises(VL.SchemaError):
        VL.write_vec(tmp_path / "x.vec", d)


def test_capture_loads_the_same_files_as_diff_hook():
    """vector_capture.PRE_PARTS must stay the list diff_hook.py loads before the fixture fragments."""
    import vector_capture
    src = (ROOT / "re" / "frida" / "diff_hook.py").read_text()
    m = re.search(r"parts = \[(.*?)\] \+ frags", src)
    names = re.findall(r'JS_DIR / "([\w.]+)"', m[1])
    assert names == vector_capture.PRE_PARTS


def test_capture_js_keeps_its_cmodule_referenced():
    """Frida frees an unreferenced CModule's code: the first capture version faulted calling getcw (2026-10-18)."""
    for name in ("vector_capture.js", "vector_replay.js"):
        js = (ROOT / "re" / "frida" / "js" / name).read_text()
        assert "new CModule(" in js and re.search(r"globalThis\.__\w+Cm = cm;", js), name


@need_exe
def test_emulator_runs_the_file_image():
    import vector_emu as EMU
    img = EMU.load_image(str(EXE))
    data = bytes(VL.DATA_SIZE)
    r = EMU.run_call(img, data, [], "default", ["int", "int"], [3, 4], "int", 0x27F, 0x00467170)   # approxDistance
    assert r.err is None and r.eax == 5 and r.pop == 0 and not r.writes
    assert {(0x00467176, 0x0046717A), (0x00467180, 0x00467184), (0x00467186, 0x00467191)} <= r.edges
    r = EMU.run_call(img, data, [(0x20000000, (0x3039).to_bytes(4, "little") + bytes(60))], "thiscall", ["pointer"],
                     [0x20000000], "double", 0x27F, 0x0045C1A0)                                            # Random::next
    assert r.err is None and r.x87 and r.st0 is not None and 0.0 <= r.st0 < 1.0
    assert set(range(0x20000000, 0x20000004)) <= r.writes


@need_exe
def test_emulator_reports_an_import():
    import vector_emu as EMU
    img = EMU.load_image(str(EXE))
    trap = next(t for t, n in img.imports.items() if n.endswith("!timeGetTime"))
    # a stub that calls the import through its IAT slot: call [slot]; ret
    slot = next(o for o in range(0x004BA000, 0x004C1000, 4) if int.from_bytes(img.data[o - 0x400000:o - 0x400000 + 4], "little") == trap)
    code = b"\xff\x15" + slot.to_bytes(4, "little") + b"\xc3"
    r = EMU.run_call(img, bytes(VL.DATA_SIZE), [(0x21000000, code)], "default", [], [], "int", 0x27F, 0x21000000)
    assert r.err and "timeGetTime" in r.err and r.imports == [img.imports[trap]]


@need_exe
def test_emu_validation_passes_a_true_vector_and_fails_a_wrong_one(tmp_path, monkeypatch):
    """vector_validate.emu_validate on a synthetic .vec of approxDistance: the stored ret reproduces; a wrong stored ret,
    a wrong out region and a missing out region read as failures (negative controls of the checker)."""
    import vector_validate as VV
    doc = {"schema": VL.SCHEMA, "function": "approxDistance", "module": "golf_clean.exe", "tooling_va": "0x00467170",
           "keys": [{"key": "approxDistance", "abi": "default", "ret": "int", "args": ["int", "int"], "fpcw": 0x27F,
                     "blocks": [], "relocs": [], "seed": [], "const": [], "vectors": []}]}
    vecs = doc["keys"][0]["vectors"]
    for i, (a, b, want) in enumerate([(3, 4, 5), (300, -400, 550), (-0x80000000, 1, -1073741823)]):
        vecs.append({"i": i, "status": "ok", "args": [{"int": a}, {"int": b}], "ret": want, "in": [], "out": [], "stack_pop": 0})
    good = tmp_path / "good.vec"
    VL.write_vec(good, doc)
    rows = VV.emu_validate([good])
    assert rows[0]["pass"] == 3 and rows[0]["fail"] == 0
    bad = copy.deepcopy(doc)
    bad["keys"][0]["vectors"][1]["ret"] = 551
    bad["keys"][0]["vectors"][2]["out"] = [{"sym": "g_rng", "off": 0, "hex": "00"}]
    p = tmp_path / "bad.vec"
    VL.write_vec(p, bad)
    rows = VV.emu_validate([p])
    assert rows[0]["pass"] == 1 and rows[0]["fail"] == 2


def test_manifest_and_selection_shape():
    sel = ROOT / "re" / "port" / "vector_selection.tsv"
    man = ROOT / "re" / "port" / "vectors_manifest.tsv"
    if not sel.exists() or not man.exists():
        pytest.skip("selection / manifest not generated")
    head = sel.read_text().splitlines()[0].split("\t")
    assert head[:5] == ["addr", "name", "subsystem", "level", "tier"] and "selected" in head and "reason" in head
    mh = man.read_text().splitlines()[0].split("\t")
    for c in ("function", "key", "vectors", "distinct_results", "branch_notes", "sha256", "bytes", "x87", "depends_on_heap"):
        assert c in mh
