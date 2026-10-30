"""NET.trace, the DirectPlay call side file of a schema v1 trace (roadmap S9.2 part 1, 2026-10-29; re/analysis/net/net_recordings.md).

Static (no game): the agent's generated tables equal what re/tools/net_trace.py derives from the DirectX 8 SDK headers
(skipped when tools/dxsdk8 is absent) and their pinned slot counts; the IDirectPlay2 slots golf_clean.exe calls at
0x00497d6a / 0x00497d75 are Close / Release; the struct offsets the agent hard-codes equal the headers'; the agent's codec
block (run in Node) writes exactly the bytes net_trace.Writer / Item.encode write, and they parse back; the validity rules
reject a file that breaks each of them; the combined agent (scenario_det + trace_rec + net_rec) parses; trace_rec.js and
trace_format.py are not changed by this side file.
"""
import json
import re
import shutil
import struct
import subprocess
import sys

import pytest

from conftest import ROOT

pytestmark = pytest.mark.static

sys.path.insert(0, str(ROOT / "re" / "tools"))
import net_trace as NT  # noqa: E402

AGENT = ROOT / "re" / "frida" / "js" / "net_rec.js"
EXE = ROOT / "original" / "golf_clean.exe"
need_sdk = pytest.mark.skipif(not NT.SDK.exists(), reason="DirectX 8 SDK headers (tools/dxsdk8) not present")
need_node = pytest.mark.skipif(shutil.which("node") is None, reason="node not on PATH")


def agent_tables() -> dict:
    block = NT.agent_block(AGENT)
    assert block is not None, "net_rec.js lost its generated block"
    return json.loads(block[block.index("{"):block.rindex("};") + 1])


@need_sdk
def test_agent_tables_equal_the_sdk_headers():
    assert NT.agent_block(AGENT) == NT.tables_js(NT.build_tables())


def test_slot_counts_and_iids():
    t = agent_tables()
    n = {x["name"]: len(x["slots"]) for x in t["ifaces"]}
    assert n == {"IUnknown": 3, "IClassFactory": 5, "IDirectPlay": 25, "IDirectPlay2": 32, "IDirectPlay2A": 32,
                 "IDirectPlay3": 47, "IDirectPlay3A": 47, "IDirectPlay4": 53, "IDirectPlay4A": 53, "IDirectPlayLobby": 14,
                 "IDirectPlayLobbyA": 14, "IDirectPlayLobby2": 15, "IDirectPlayLobby2A": 15, "IDirectPlayLobby3": 19,
                 "IDirectPlayLobby3A": 19}
    iid = {x["name"]: x["iid"] for x in t["ifaces"]}
    assert iid["IDirectPlay2A"] == "9d460580-a822-11cf-960c-0080c7534e82"
    assert iid["IDirectPlayLobbyA"] == "26c66a70-b367-11cf-a024-00aa006157ac"
    assert len(set(iid.values())) == len(iid)
    # a later interface extends the earlier one: its first slots are the same methods
    names = {x["name"]: [s[0] for s in x["slots"]] for x in t["ifaces"]}
    assert names["IDirectPlay4A"][:47] == names["IDirectPlay3A"] and names["IDirectPlay3A"][:32] == names["IDirectPlay2A"]


def slot(t, iface, method):
    x = next(i for i in t["ifaces"] if i["name"] == iface)
    return next((k, s) for k, s in enumerate(x["slots"]) if s[0] == method)


def test_parameter_kinds_of_the_calls_that_carry_payload():
    t = agent_tables()
    assert slot(t, "IDirectPlay2A", "Send") == (26, ["Send", ["DPID", "DPID", "DWORD", "LPVOID", "DWORD"], ["u", "u", "u", "b4", "u"]])
    assert slot(t, "IDirectPlay2A", "Receive")[1][2] == ["i", "i", "u", "B4", "i"]
    assert slot(t, "IDirectPlay2A", "EnumSessions")[1][2] == ["sd", "u", "c:LPDPENUMSESSIONSCALLBACK2", "p", "u"]
    assert slot(t, "IDirectPlay2A", "EnumPlayers")[1][2] == ["g", "c:LPDPENUMPLAYERSCALLBACK2", "p", "u"]   # ctx is not a buffer
    assert slot(t, "IDirectPlay2A", "GetPlayerName")[1][2] == ["u", "B2", "i"]
    assert slot(t, "IDirectPlay2A", "QueryInterface")[1][2] == ["g", "o@0"]
    assert slot(t, "IDirectPlayLobbyA", "Connect")[1][2] == ["u", "o:IDirectPlay2A", "U"]
    assert slot(t, "IDirectPlayLobby", "Connect")[1][2] == ["u", "o:IDirectPlay2", "U"]
    assert slot(t, "IDirectPlayLobbyA", "EnumAddress")[1][2] == ["c:LPDPENUMADDRESSCALLBACK", "b2", "u", "p"]
    ex = {e[0]: e[2] for e in t["exports"]}
    assert ex["DirectPlayCreate"] == ["g", "o:IDirectPlay", "U"]
    assert ex["DirectPlayLobbyCreateA"] == ["g", "o:IDirectPlayLobbyA", "U", "b4", "u"]
    assert ex["DllGetClassObject"] == ["g", "g", "o@1"]
    cbs = {c[0]: c[2] for c in t["cbs"]}
    assert cbs["LPDPENUMSESSIONSCALLBACK2"] == ["sd", "i", "u", "p"]


@pytest.mark.skipif(not EXE.exists(), reason="golf_clean.exe not present")
def test_the_exe_calls_close_and_release_through_the_idirectplay2_slots():
    """0x00497d62..0x00497d78: eax = [0x008400b0]; call [edx + 0x10] (Close); call [ecx + 8] (Release); [0x008400b0] = 0."""
    import pefile
    pe = pefile.PE(str(EXE), fast_load=True)
    img = pe.get_memory_mapped_image()
    at = lambda va, n: img[va - 0x400000:va - 0x400000 + n]
    assert at(0x00497d62, 5) == b"\xa1\xb0\x00\x84\x00" and at(0x00497d6a, 3) == b"\xff\x52\x10"
    assert at(0x00497d6d, 5) == b"\xa1\xb0\x00\x84\x00" and at(0x00497d75, 3) == b"\xff\x51\x08"
    assert at(0x00497d78, 6) == b"\x89\x1d\xb0\x00\x84\x00"
    t = agent_tables()
    assert slot(t, "IDirectPlay2A", "Close")[0] == 0x10 // 4 and slot(t, "IDirectPlay2A", "Release")[0] == 8 // 4


@pytest.mark.skipif(not EXE.exists(), reason="golf_clean.exe not present")
def test_the_exe_imports_no_directplay_and_names_the_dll_once():
    import pefile
    pe = pefile.PE(str(EXE))
    dlls = sorted(e.dll.decode().lower() for e in pe.DIRECTORY_ENTRY_IMPORT)
    assert dlls == ["binkw32.dll", "kernel32.dll", "terrain.dll", "user32.dll", "winmm.dll"]
    k32 = {i.name.decode(): i.address for e in pe.DIRECTORY_ENTRY_IMPORT if e.dll.lower() == b"kernel32.dll" for i in e.imports}
    assert k32["GetProcAddress"] == 0x004ba0a4 and k32["LoadLibraryA"] == 0x004ba0a8      # the slots net_rec.js wraps
    src = AGENT.read_text(encoding="utf-8")
    assert "GetProcAddress: 0x004ba0a4, LoadLibraryA: 0x004ba0a8" in src
    data = EXE.read_bytes()
    assert data.count(b"DPLAYX.dll") == 1 and b"DirectPlay" not in data


@need_sdk
def test_struct_offsets_the_agent_uses_equal_the_headers():
    sd, nm, cn = (NT.struct_offsets(NT.SDK, s) for s in ("DPSESSIONDESC2", "DPNAME", "DPLCONNECTION"))
    assert (sd["__size__"], sd["lpszSessionName"], sd["lpszPassword"]) == (0x50, 0x30, 0x34)
    assert (nm["__size__"], nm["lpszShortName"], nm["lpszLongName"]) == (0x10, 0x8, 0xc)
    assert (cn["__size__"], cn["lpSessionDesc"], cn["lpPlayerName"], cn["lpAddress"], cn["dwAddressSize"]) == (0x28, 8, 0xc, 0x20, 0x24)
    src = AGENT.read_text(encoding="utf-8")
    assert "nrStructItem(NR_SK.DPSESSIONDESC2, p, 0x50, [[0x30," in src and "[0x34," in src
    assert "nrStructItem(NR_SK.DPNAME, p, 0x10, [[0x8," in src and "[0xc, (q) => nrStrItem(q, w)]]" in src
    assert "p.add(0x24).readU32()" in src and "nrStructItem(NR_SK.DPLCONNECTION, p, 0x28, [[0x8," in src and "[0x20," in src


# ---------------------------------------------------------------- the agent's codec in Node == net_trace's encoder
def codec_block() -> str:
    s = AGENT.read_text(encoding="utf-8")
    a = s.index("// <nr-codec>")
    return s[s.index("\n", a) + 1:s.index("// </nr-codec>")]


DRIVER = r"""
const hex = (a) => Buffer.from(a).toString('hex');
const sd = nrIt.struct(NR_SK.DPSESSIONDESC2, Array.from({length: 0x50}, (_, i) => i), [nrIt.str([0x47, 0x6f]), nrIt.nul()]);
const call = nrPack(NR_T.CALL, 0, nrP.call(7, 0, 0, 0x00497d6d, 5, 26, 0, [nrIt.u32(1), nrIt.u32(0), nrIt.u32(0),
  nrIt.bytes([1, 2, 3]), nrIt.u32(3)]));
const cb = nrPack(NR_T.CB, 0, nrP.cb(8, 1, 1, 0x00499000, 9, [sd, nrIt.u32(1500), nrIt.u32(0), nrIt.ptr()]));
const rel = nrRelocate([0x08, 0, 0x50, 0, 0x04, 0, 0x50, 0, 0xff, 0xff, 0xff, 0xff], 0x500000);
const ret = nrPack(NR_T.RET, 0, nrP.ret(9, 0, 0, 0, [[0, nrIt.obj(2)], [3, nrIt.rel(rel[0], rel[1])], [4, nrIt.u32(12)]]));
const misc = nrCat([nrPack(NR_T.MODULE, 1, nrP.module(3, 0, 'dplayx.dll')),
  nrPack(NR_T.LOADLIB, 0, nrP.loadlib(3, 0, 0x004a00aa, 1, 'dplayx.dll')),
  nrPack(NR_T.GETPROC, 0, nrP.getproc(3, 0, 0x004a00c0, 1, 1, 1, '', 0)),
  nrPack(NR_T.NEWOBJ, 0, nrP.newobj(9, 0, 3, 0, 1)), nrPack(NR_T.END, 1, nrP.end(5, 1, 1, 1)),
  nrIt.guid(Array.from({length: 16}, (_, i) => 0xa0 + i)), nrIt.wstr([0x41, 0, 0x42, 0]), nrIt.cb(0x80000000 | 0x1234),
  nrIt.bad(), nrIt.outnull()]);
console.log(JSON.stringify({call: hex(call), cb: hex(cb), ret: hex(ret), rel: [rel[0], rel[1]], misc: hex(misc)}));
"""


@pytest.fixture(scope="module")
def node_out():
    if shutil.which("node") is None:
        pytest.skip("node not on PATH")
    r = subprocess.run(["node", "-e", codec_block() + DRIVER], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    return json.loads(r.stdout)


I = NT.Item
SD = I("STRUCT", (1, bytes(range(0x50)), [I("STR", "Go"), I("NULL")]))


def rec(name, sub, payload):
    return struct.pack("<BBI", NT.RECN[name], sub, len(payload)) + payload


def p_call(pos, seq, thr, ra, iface, slot_, obj, items):
    return struct.pack("<IIBIBBI", pos, seq, thr, ra, iface, slot_, obj) + NT.enc_items(items)


def p_cb(pos, seq, thr, fn, cb, items):
    return struct.pack("<IIBIB", pos, seq, thr, fn, cb) + NT.enc_items(items)


def p_ret(pos, seq, thr, ret, outs):
    return struct.pack("<IIBI", pos, seq, thr, ret) + NT.enc_items(outs, indexed=True)


def s8(s):
    return bytes([len(s)]) + s.encode("latin-1")


def test_codec_call_cb_ret_bytes_equal_the_reader_encoder(node_out):
    call = rec("CALL", 0, p_call(7, 0, 0, 0x00497d6d, 5, 26, 0, [I("U32", 1), I("U32", 0), I("U32", 0), I("BYTES", b"\1\2\3"), I("U32", 3)]))
    cb = rec("CB", 0, p_cb(8, 1, 1, 0x00499000, 9, [SD, I("U32", 1500), I("U32", 0), I("PTR")]))
    # the 12-byte out buffer at 0x500000 holds two pointers into itself (+0x8, +0x4) and one outside (0xffffffff)
    relb = struct.pack("<III", 0x8, 0x4, 0xffffffff)
    ret = rec("RET", 0, p_ret(9, 0, 0, 0, [(0, I("OBJ", 2)), (3, I("RELBYTES", (relb, [0, 4]))), (4, I("U32", 12))]))
    assert node_out["call"] == call.hex() and node_out["cb"] == cb.hex() and node_out["ret"] == ret.hex()
    assert node_out["rel"] == [list(relb), [0, 4]]


def test_codec_other_records_and_items_equal_the_reader_encoder(node_out):
    misc = (rec("MODULE", 1, struct.pack("<IB", 3, 0) + s8("dplayx.dll"))
            + rec("LOADLIB", 0, struct.pack("<IBIB", 3, 0, 0x004a00aa, 1) + s8("dplayx.dll"))
            + rec("GETPROC", 0, struct.pack("<IBIBBI", 3, 0, 0x004a00c0, 1, 1, 1) + s8("") + b"\0")
            + rec("NEWOBJ", 0, struct.pack("<IIBIB", 9, 0, 3, 0, 1)) + rec("END", 1, struct.pack("<4I", 5, 1, 1, 1))
            + I("GUID", bytes(range(0xa0, 0xb0))).encode() + I("WSTR", "AB").encode() + I("CB", 0x80001234).encode()
            + I("BADPTR").encode() + I("OUTNULL").encode())
    assert node_out["misc"] == misc.hex()


def synthetic(records: bytes, **meta) -> bytes:
    m = NT.meta_for(agent_tables(), "synthetic", NT.agent_block(AGENT))
    m.update(meta)
    return NT.header_bytes(m) + records


def good_body(node_out) -> bytes:
    """MODULE, LOADLIB, GETPROC, CALL DirectPlayCreate (seq 0), NEWOBJ 0, RET, CALL Send on obj 0 (seq 1), CB (seq 2),
    CBRET, RET, END: a session fragment whose records obey every rule."""
    t = agent_tables()
    ex = [e[0] for e in t["exports"]].index("DirectPlayCreate")
    ifc = [x["name"] for x in t["ifaces"]].index("IDirectPlay2A") + 1
    b = bytes.fromhex(node_out["misc"])
    b = b[:b.index(rec("NEWOBJ", 0, struct.pack("<IIBIB", 9, 0, 3, 0, 1)))]          # MODULE, LOADLIB, GETPROC
    b += rec("CALL", 0, p_call(4, 0, 0, 0x004a00d3, 0, ex, 0xFFFFFFFF, [I("NULL"), I("PTR"), I("NULL")]))
    b += rec("NEWOBJ", 0, struct.pack("<IIBIB", 4, 0, ifc, 0, 1))
    b += rec("RET", 0, p_ret(4, 0, 0, 0, [(1, I("OBJ", 0))]))
    b += rec("CALL", 0, p_call(7, 1, 0, 0x00497d6d, ifc, 26, 0, [I("U32", 1), I("U32", 0), I("U32", 0), I("BYTES", b"x"), I("U32", 1)]))
    b += rec("CB", 0, p_cb(7, 2, 1, 0x00499000, 9, [SD, I("U32", 1500), I("U32", 0), I("PTR")]))
    b += rec("CBRET", 0, p_ret(7, 2, 1, 1, [(1, I("U32", 0))]))
    b += rec("RET", 0, p_ret(8, 1, 0, 0, []))
    return b + rec("END", 0, struct.pack("<4I", 10, 2, 1, 1))


def test_a_valid_fragment_parses_validates_and_summarises(node_out, tmp_path):
    data = synthetic(good_body(node_out))
    tr = NT.parse(data)
    assert NT.validate(tr) == []
    s = NT.summary(tr)
    assert s["by_type"] == {"MODULE": 1, "LOADLIB": 1, "GETPROC": 1, "CALL": 2, "NEWOBJ": 1, "RET": 2, "CB": 1, "CBRET": 1, "END": 1}
    assert s["by_method"] == {"DirectPlayCreate": 1, "IDirectPlay2A::Send": 1} and s["sends"] == 1 and s["send_bytes"] == 1
    assert s["threads"] == [0, 1]
    # every record re-encodes to its own bytes through Item.encode (the reader loses nothing)
    sd = next(r for r in tr.records if r.name == "CB").f["args"][0]
    assert sd == SD and sd.encode() == SD.encode()
    f = tmp_path / "a.NET.trace"
    f.write_bytes(data)
    assert NT.main(["validate", str(f)]) == 0 and NT.main(["dump", str(f), "--types", "CALL"]) == 0
    assert NT.main(["diff", str(f), str(f)]) == 0


def mutate(body: bytes, i: int, fn) -> bytes:
    """Re-encode record i of a body through fn(type, sub, payload) -> bytes (b'' drops it)."""
    out, o, k = b"", 0, 0
    while o < len(body):
        t, sub, n = struct.unpack_from("<BBI", body, o)
        p = body[o + 6:o + 6 + n]
        out += fn(t, sub, p) if k == i else body[o:o + 6 + n]
        o, k = o + 6 + n, k + 1
    return out


def setb(off, fmt, v):
    return lambda t, sub, p: struct.pack("<BBI", t, sub, len(p)) + p[:off] + struct.pack(fmt, v) + p[off + struct.calcsize(fmt):]


CONTROLS = [
    ("pos goes back", 6, setb(0, "<I", 2), "pos 2 <"),
    ("call seq skips", 6, setb(4, "<I", 5), "seq 5 != 1"),
    ("RET of no open CALL", 9, setb(4, "<I", 7), "has no open CALL"),
    ("CALL on an object not created", 6, setb(15, "<I", 3), "object 3 not created"),
    ("slot outside the interface", 6, setb(14, "<B", 99), "outside the tables"),
    ("thread ordinal skips", 7, setb(8, "<B", 3), "thread ordinal 3 skips"),
    ("NEWOBJ out of order", 4, setb(4, "<I", 1), "NEWOBJ 1 != 0"),
    ("END totals", 10, setb(0, "<I", 11), "END totals"),
    ("END dropped", 10, lambda t, sub, p: b"", "no END record"),
]


@pytest.mark.parametrize("name,i,fn,msg", CONTROLS, ids=[c[0] for c in CONTROLS])
def test_each_rule_rejects_a_file_that_breaks_it(node_out, name, i, fn, msg):
    errs = NT.validate(NT.parse(synthetic(mutate(good_body(node_out), i, fn))))
    assert any(msg in e for e in errs), errs


def test_parse_rejects_bad_files(node_out):
    good = synthetic(good_body(node_out))
    for bad in (b"XXXXXXXX" + good[8:], good[:-3], good[:20] + b"[" + good[21:]):
        with pytest.raises(NT.NetTraceError):
            NT.parse(bad)
    with pytest.raises(NT.NetTraceError):
        NT.parse(synthetic(rec("END", 0, struct.pack("<4I", 0, 0, 0, 0))[:-1] + b"\0\0"))     # trailing bytes


def test_diff_names_the_first_differing_record(node_out):
    a = NT.parse(synthetic(good_body(node_out)))
    b = NT.parse(synthetic(mutate(good_body(node_out), 6, setb(0, "<I", 8))))
    d = NT.diff(a, b)
    assert d["where"] == 6 and "IDirectPlay2A::Send" in d["a"]


def test_relocation_rewrites_only_pointers_into_the_buffer(node_out):
    b, fix = node_out["rel"]
    assert fix == [0, 4] and b[8:] == [0xff] * 4        # 0x500008 -> 0x8, 0x500004 -> 0x4, 0xffffffff untouched


@need_node
def test_the_combined_agent_parses():
    import net_rec
    assert net_rec.cmd_check(None) == 0


def test_the_agent_is_opt_in_and_keys_to_trace_rec():
    """net_rec.js is loaded only by net_rec.py, after trace_rec.js; it ends with trace_rec's END and never writes pointers."""
    src = AGENT.read_text(encoding="utf-8")
    assert "needs trace_rec.js appended before it" in src and "trEnd = function (byExit)" in src
    for f in ("re/frida/js/scenario_det.js", "re/frida/js/trace_rec.js", "re/frida/scenario_ab.py", "re/tools/trace_tool.py",
              "re/tools/scenario_refs.py"):
        assert "net_rec" not in (ROOT / f).read_text(encoding="utf-8"), f
    code = src[src.index("// </nr-codec>"):]
    # what reaches nrEmit: positions, ordinals, call sites (trSite), bytes; a raw pointer would be a .toUInt32() of an
    # argument outside trSite / nrLE32 of a size or ordinal: every toUInt32() is listed here
    uses = re.findall(r"[\w\]\)\.\[]+\.toUInt32\(\)", code)
    allowed = {"a.toUInt32()", "rv.toUInt32()", "c.toUInt32()", "fn.toUInt32()", "ra.toUInt32()", "this.returnAddress.toUInt32()",
               "10)].toUInt32()", "a[1].toUInt32()"}   # 10)]: args[base + parseInt(k.slice(1), 10)], a size
    assert set(uses) <= allowed, set(uses) - allowed


NEED = ROOT / "re" / "port" / "net_functions.tsv"


@need_sdk
@pytest.mark.skipif(not NEED.exists(), reason="S9.1's re/port/net_functions.tsv not written yet")
def test_every_directplay_entry_point_the_survey_names_is_in_the_tables():
    names = NT.table_names(NT.build_tables())
    need = NT.need_list(NEED)
    assert need, "the survey names no DirectPlay call: the check would pass vacuously"
    missing = [n for n in need if n not in names]
    assert not missing, missing
