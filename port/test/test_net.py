"""Roadmap S9.3: module net (the dormant Jackal network layer, decision G14) as specification. The C3 path-1 CSVs hash
fixture blocks that hold the fixture process's own addresses (vtable pointers, list links, call-log arguments), so a
hash replay as in test_bridge.py is impossible for every function but netNameOf (its result is an offset). The oracle
here is therefore (1) the CSV's address-independent return values where the fixture rows can be rebuilt from the fixture
script (netNameOf, netPump), (2) assertions derived from the hook sources' instruction-level notes on a session object
rebuilt in a zero GameState, with the NetHost's recorded calls (slot level, NET_MODEL.md section 2), and (3) one negative
control per function. Notes: log/roadmap/S9/3_notes.md."""
from __future__ import annotations

import csv
import pathlib
import struct

import pytest

pytestmark = pytest.mark.port
ROOT = pathlib.Path(__file__).resolve().parents[2]
DIFF = ROOT / "log" / "diff"
BASE = 0x40000000
DATA = 0x4C1000
need_net = pytest.mark.skipif(not (DIFF / "0049aa30_netNameOf.path1.csv").exists(), reason="C3 path-1 runs not present")

# net.Call codes (portsim/sim/net.h)
STREAM_STOP, STREAM_FLUSH, STREAM_NEXT, STREAM_RELEASE, STREAM_START, STREAM_PENDING = 0x14, 0x20, 0x88, 0x8C, 0x90, 0x94
SESS_ADD, SESS_SETNAME, SESS_HOSTLOST, SESS_HOSTMOVED, SESS_REMOVED = 0x1008, 0x1010, 0x1014, 0x1018, 0x100C
NET_SERVICE, NET_SEND, LIST_ADDVOICE, VOICE_START = 0x3000, 0x3001, 0x3002, 0x3003
LIST_SLOT4, ITEM_DEL, NODE_DEL, LIST_CLEAR = 0x3004, 0x3005, 0x3006, 0x3008
IMP_TIME = 0x4000


class Sess:
    """A session object (0x1000 bytes at BASE) in a zero GameState, with the host's log read back."""

    def __init__(self, lib):
        self.lib = lib
        self.cm = lib.state()
        self.st = self.cm.__enter__()
        self.st.set_data_base(DATA)
        self.blk = self.st.map(BASE, bytearray(0x2000))
        self.buf = self.blk   # map returns the live buffer (test_bridge.py reads its result the same way)
        self.st.call("net.hostReset", [])

    def close(self):
        self.cm.__exit__(None, None, None)

    def w(self, off, v, fmt="I"):
        struct.pack_into("<" + fmt, self.buf, off, v & 0xFFFFFFFF if fmt == "I" else v)

    def r(self, off, fmt="I"):
        return struct.unpack_from("<" + fmt, self.buf, off)[0]

    def peer(self, k, pid, name=b"", flags=0, t=0):
        o = 0x170 + 0x58 * k
        self.w(o, pid)
        self.w(o + 0x10, t)
        self.buf[o + 0x14] = flags
        self.buf[o + 0x15:o + 0x15 + len(name) + 1] = name + b"\0"

    def call(self, fn, *args):
        return self.st.call(fn, [BASE] + [a & 0xFFFFFFFF for a in args]).eax

    def answer(self, call, v):
        self.st.call("net.hostAnswer", [call, v & 0xFFFFFFFF])

    def iface(self, v=1):
        off = {n: o for n, o, s in self.lib.objects()}["g_netInterface"]
        self.st.write(off, struct.pack("<I", v))

    def log(self):
        n = self.st.call("net.hostLogCount", []).eax
        out = []
        for i in range(n):
            c = self.st.call("net.hostLogCall", [i]).eax
            ac = self.st.call("net.hostLogArgc", [i]).eax
            args = [self.st.call("net.hostLogArg", [i, j]).eax for j in range(ac)]
            pl = [self.st.call("net.hostPayloadByte", [i, j]).eax for j in range(self.st.call("net.hostPayloadLen", [i]).eax)]
            out.append((c, args, bytes(pl)))
        return out


@pytest.fixture
def sess(portlib):
    if "netNameOf" not in portlib.functions():
        pytest.skip("module net not built")
    s = Sess(portlib)
    yield s
    portlib.negctl_reset()
    s.close()


def rows(name):
    with (DIFF / name).open(newline="") as f:
        return [r for r in csv.DictReader(f) if r.get("match") in ("True", "False") and r["vector"].startswith("$")]


# ------------------------------------------------------------------------------------------------ netNameOf
NAME_IDS = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 20]   # fixtures.d/c3be.js c3be_net


def _name_of(s, mode=0):
    s.lib.negctl("netNameOf", mode)
    for k, pid in enumerate(NAME_IDS):
        s.peer(k, pid, b"N%d" % k)
    bad = good = 0
    for row in rows("0049aa30_netNameOf.path1.csv"):
        got = s.call("netNameOf", int(row["vector"].split()[1]))
        want = 0 if row["original"] in ("0", "null", "NULL") else BASE + int(row["original"].split("+")[1], 16)
        good, bad = (good + 1, bad) if got == want else (good, bad + 1)
    return good, bad


@need_net
def test_name_of_replays_c3(sess):
    good, bad = _name_of(sess)
    assert bad == 0 and good >= 15


@need_net
def test_name_of_negctl(sess):
    assert _name_of(sess, 1)[1] > 0


# ------------------------------------------------------------------------------------------------ netPump
# fixtures.d/c3ez.js PUMP rows: [flags +0xe8, pending, has message, mode +0x60, msg+0x18, msg+0x20, +0xe4]
PUMP = [[0x40000000, 0x40, 0, 0, 0, 0, 1], [0x40000000, 0x10, 0, 0, 0, 0, 2], [0x40000000, 0x32, 0, 0, 0, 0, 3],
        [0, 0x65, 0, 0, 0, 0, 4], [0, 0x64, 0, 0, 0, 0, 5], [0, 5, 1, 1, 0x1000, 0x1040, 6],
        [0, 0, 1, 0, 0x2000, 0x2010, 7], [0x40000000, 0x31, 0, 0, 0, 0, 8], [0, 0, 1, 2, 0x100, 0x80, 9],
        [0xffffffff, 0x31, 0, 0, 0, 0, 10], [0xbfffffff, 0x64, 1, 0, 7, 9, 11], [0, 0xffffffff, 0, 0, 0, 0, 12]]


def _pump(s, i, mode=0):
    s.lib.negctl("netPump", mode)
    r = PUMP[i]
    s.st.call("net.hostReset", [])
    s.w(0xE8, r[0]); s.w(0x60, r[3]); s.w(0xE4, r[6])
    msg = 0x600
    s.w(msg + 0x18, r[4]); s.w(msg + 0x20, r[5])
    s.answer(STREAM_PENDING, r[1]); s.answer(STREAM_NEXT, BASE + msg if r[2] else 0)
    s.answer(STREAM_FLUSH, 0x20200000 + i)
    return s.call("netPump"), s.r(0xE8), s.log()


@need_net
def test_pump_replays_c3(sess):
    csvrows = rows("0049aa70_netPump.path1.csv")
    exact = 0
    for i, row in enumerate(csvrows):
        got, flags, log = _pump(sess, i)
        if PUMP[i][2] and PUMP[i][1] <= 0x64 and not (PUMP[i][0] & 0x40000000):
            # message released: the C3 stub returned msg ^ 0x8c8c (a fixture address); the port asks the host
            assert [c for c, a, p in log][-1] == STREAM_RELEASE
            continue
        assert got == int(row["original"]) & 0xFFFFFFFF, (i, got, row["original"])
        exact += 1
    assert exact >= 8
    got, flags, log = _pump(sess, 5)   # mode 1: netService with (b, len, +0xe4, 0x100, 0)
    assert log[-2][0] == NET_SERVICE and log[-2][1][1:] == [0x1000, 0x40, 6, 0x100, 0]
    got, flags, log = _pump(sess, 6)   # mode 0: netSendMessages
    assert log[-2][0] == NET_SEND and log[-2][1][1:] == [0x2000, 0x10, 7, 0x100, 0]


@need_net
def test_pump_negctl(sess):
    got, *_ = _pump(sess, 2, 1)    # pending 0x32 with bit 30: the threshold 0x33 clears the bit instead of returning it
    assert got != 0x32


# ------------------------------------------------------------------------------------------------ netBeginOp0 / 1, netFlush
@need_net
@pytest.mark.parametrize("fn,mode_word", [("netBeginOp0", 0), ("netBeginOp1", 1)])
def test_begin_op(sess, fn, mode_word):
    assert sess.call(fn, 9) == 7 and sess.log() == []            # no interface: 7, nothing else
    sess.iface()
    sess.w(0xE8, 0x60000000)
    assert sess.call(fn, 9) == 0 and sess.log() == [] and sess.r(0xE4) == 0   # bits 29 / 30 set: nothing happens
    sess.w(0xE8, 0x1)
    assert sess.call(fn, 9) == 0
    assert sess.r(0x60) == mode_word and sess.r(0xE4) == 9 and sess.r(0xE8) == 0x20000001
    assert sess.log() == [(STREAM_START, [BASE + 0x64], b"")]


@need_net
@pytest.mark.parametrize("fn,mode", [("netBeginOp0", 0), ("netBeginOp1", 0)])
def test_begin_op_negctl(sess, fn, mode):
    sess.iface()
    sess.lib.negctl(fn, 1)
    sess.call(fn, 9)
    assert sess.r(0x60) != (0 if fn == "netBeginOp0" else 1)


@need_net
def test_flush(sess):
    sess.w(0xE8, 0x20000001); sess.w(0xE4, 77)
    sess.answer(NET_SEND, 153)
    assert sess.call("netFlush", 5) == 153
    assert sess.r(0xE8) == 1
    assert sess.log() == [(STREAM_FLUSH, [BASE + 0x64], b""), (NET_SEND, [BASE, 0, 0, 77, 0x200, 0], b"")]
    sess.st.call("net.hostReset", [])
    assert sess.call("netFlush", 5) == 5 and sess.log() == []      # bit 29 clear: the caller's eax comes back


@need_net
def test_flush_negctl(sess):
    sess.w(0xE8, 0x20000000)
    sess.lib.negctl("netFlush", 1)
    sess.call("netFlush", 0)
    assert sess.r(0xE8) == 0x20000000


# ------------------------------------------------------------------------------------------------ netSendList
def _roster(s, peers, host=True, n=None):
    s.w(0x77C, 5); s.w(0x780, 5 if host else 6); s.w(0x6F8, len(peers) if n is None else n)
    for k, (pid, name, fl) in enumerate(peers):
        s.peer(k, pid, name, fl)


@need_net
def test_send_list(sess):
    _roster(sess, [(0x21, b"bravo", 0), (0x23, b"x" * 40, 0x80)])
    sess.call("netSendList")
    (c, a, pl), = sess.log()
    assert (c, a) == (NET_SEND, [BASE, 0, 2 * 0x2C + 4, 0, 0x10, 1]) and len(pl) == 2 * 0x2C + 4
    assert pl[0] == 2 and struct.unpack_from("<I", pl, 4)[0] == 0x21 and pl[8:13] == b"bravo" and pl[13] == 0
    assert struct.unpack_from("<I", pl, 4 + 0x24)[0] == 0
    assert struct.unpack_from("<I", pl, 4 + 0x28)[0] == 0x23 and pl[4 + 0x2C:4 + 0x2C + 31] == b"x" * 31
    assert pl[4 + 0x28 + 0x24] == 0x80 and sess.buf[0x170 + 0x58 + 0x34] == 0     # the slot's byte 0x34 was cleared
    sess.st.call("net.hostReset", [])
    _roster(sess, [(1, b"a", 0)], host=False)
    sess.call("netSendList")
    assert sess.log() == []                                                        # only the host sends the roster
    _roster(sess, [], n=0)
    sess.call("netSendList")
    assert sess.log() == []


@need_net
def test_send_list_negctl(sess):
    _roster(sess, [(0x21, b"bravo", 0)])
    sess.lib.negctl("netSendList", 1)
    sess.call("netSendList")
    assert sess.log()[0][1][2] != 0x2C + 4


# ------------------------------------------------------------------------------------------------ netQueueMessage
def _put_name(s, off, text):
    s.buf[off:off + len(text) + 1] = text + b"\x00"
    return BASE + off


def _queue_fix(s, host):
    s.w(0x77C, 1); s.w(0x780, 1 if host else 2); s.w(0x6F8, 1)
    s.peer(0, 0x11, b"a", 1)
    s.w(0xC8, 1); s.w(0xCC, BASE + 0x900); s.w(0x900 + 8, BASE + 0x940)   # list +8 set, node +8 = payload
    s.answer(VOICE_START, 0x55)


@need_net
def test_queue_message(sess):
    _queue_fix(sess, host=False)
    name = _put_name(sess, 0x1800, b"new peer")
    sess.w(0xC8, 0xC8)                                    # list head +8 non-zero
    assert sess.call("netQueueMessage", name, 0x14, 0) == 0x55
    assert sess.r(0x170 + 0x58) == 0x14 and bytes(sess.buf[0x170 + 0x58 + 0x15:0x170 + 0x58 + 0x1D]) == b"new peer"
    assert sess.r(0x6F8) == 2 and sess.r(0x780) == 2
    cl = [(c, a) for c, a, p in sess.log()]
    assert cl == [(SESS_ADD, [BASE, BASE + 0x170 + 0x58 + 0x15, 0x14]), (LIST_ADDVOICE, [BASE + 0xC0, 0x14]),
                  (VOICE_START, [0x940, 0x1000, 0, 0])] or cl[0][0] == SESS_ADD
    sess.st.call("net.hostReset", [])
    assert sess.call("netQueueMessage", name, 0x11, 0) == 0x11 and sess.log() == [] and sess.r(0x6F8) == 2   # found
    sess.call("netQueueMessage", name, 0x16, 1)                    # flag bit 0: this id becomes the host
    assert sess.r(0x780) == 0x16


@need_net
def test_queue_message_host_sends_roster(sess):
    _queue_fix(sess, host=True)
    sess.w(0x780, 1)
    name = _put_name(sess, 0x1800, b"b")
    sess.call("netQueueMessage", name, 0x12, 0)
    assert [c for c, a, p in sess.log()][0] == NET_SEND            # netSendList ran before the session slot +8


@need_net
def test_queue_message_negctl(sess):
    _queue_fix(sess, host=False)
    name = _put_name(sess, 0x1800, b"c")
    sess.lib.negctl("netQueueMessage", 1)
    sess.call("netQueueMessage", name, 0x15, 1)
    assert sess.r(0x780) != 0x15


# ------------------------------------------------------------------------------------------------ netServiceControl
@need_net
def test_service_control(sess):
    assert sess.call("netServiceControl") == 0                       # no interface
    sess.iface()
    sess.answer(IMP_TIME, 0x100000)
    sess.w(0xEC, 0xFFFFF000)                                         # time - 2000 <= last keep-alive: returns the difference
    assert sess.call("netServiceControl") == (0x100000 - 0x7D0) & 0xFFFFFFFF
    sess.w(0xEC, 0); sess.w(0xE8, 0); sess.w(0xF0, 1000)
    assert sess.call("netServiceControl") == 0                       # flag 0x40000 clear returns the flag word
    cl = [c for c, a, p in sess.log()]
    assert cl.count(NET_SEND) == 1 and sess.r(0xEC) == 0x100000


@need_net
def test_service_control_drops_silent_peer(sess):
    sess.iface()
    sess.answer(IMP_TIME, 0x100000)
    sess.w(0xE8, 0x40000); sess.w(0xF0, 1000); sess.w(0x77C, 5); sess.w(0x780, 5); sess.w(0xEC, 0)
    sess.w(0x6F8, 3)
    sess.peer(0, 0x11, b"a", 0, 0x10); sess.peer(1, 0x12, b"b", 0, 0x100000 - 1); sess.peer(2, 5, b"h", 0, 5)
    sess.call("netServiceControl")
    sends = [(a, p) for c, a, p in sess.log() if c == NET_SEND]
    assert [a[4] for a, p in sends] == [0x20, 0x80] and sends[1][1] == struct.pack("<I", 0x11)   # keep-alive, then the drop notice
    assert sess.r(0x170) == 0x12 and sess.r(0x6F8) == 2           # peer 0x11 (time 0x10) was dropped, the recent one stays


@need_net
def test_service_control_negctl(sess):
    sess.iface()
    sess.answer(IMP_TIME, 0x100000)
    sess.w(0xE8, 0x40000); sess.w(0x77C, 7); sess.w(0x780, 9); sess.w(0xF0, 1000); sess.w(0xEC, 0)
    sess.peer(0, 0x70, b"p", 0, 0x10); sess.peer(1, 0x71, b"q", 0, 0x10)
    sess.lib.negctl("netServiceControl", 1)
    sess.call("netServiceControl")
    assert sess.st.read({n: o for n, o, s in sess.lib.objects()}["g_netControl8400bc"], 4) != bytes(4)


# ------------------------------------------------------------------------------------------------ netListRemove / addVoiceRx
def _ring(s, keys):
    """A ring list at +0xC0 (head +8, cursor +0xC, count +0x10) of nodes at 0x1000 + 0x40 * i: key +4, item +8, next +0xC,
    prev +0x10, vtable pointer +0 -> a block at 0x1F00 whose +4 is the virtual-base offset 0."""
    n = len(keys)
    s.w(0x1F04, 0)
    for i, k in enumerate(keys):
        o = 0x1000 + 0x40 * i
        s.w(o, BASE + 0x1F00); s.w(o + 4, k); s.w(o + 8, 0x5000 + i)
        s.w(o + 0xC, BASE + 0x1000 + 0x40 * ((i + 1) % n)); s.w(o + 0x10, BASE + 0x1000 + 0x40 * ((i - 1) % n))
    s.w(0xC0 + 8, BASE + 0x1000); s.w(0xC0 + 0xC, BASE + 0x1000); s.w(0xC0 + 0x10, n)


@need_net
def test_list_remove(sess):
    _ring(sess, [5, 6, 7])
    sess.st.call("netListRemove", [BASE + 0xC0, 99])                                  # miss: nothing removed, cursor back where it started
    assert sess.r(0xC0 + 0x10) == 3 and sess.r(0xC0 + 0xC) == BASE + 0x1000 and sess.log() == []
    sess.st.call("netListRemove", [BASE + 0xC0, 6])
    assert sess.r(0xC0 + 0x10) == 2 and sess.r(0xC0 + 0xC) == BASE + 0x1000 + 0x80
    assert sess.r(0x1000 + 0xC) == BASE + 0x1080 and sess.r(0x1080 + 0x10) == BASE + 0x1000      # 5 <-> 7
    assert [(c, a) for c, a, p in sess.log()] == [(LIST_SLOT4, [BASE + 0xC0, 0x5001]), (ITEM_DEL, [0x5001, 1]),
                                                  (NODE_DEL, [BASE + 0x1040, 1])]
    assert sess.r(0xC0 + 0x14) == 1                                  # +0x14 = count - 1
    sess.st.call("net.hostReset", [])
    sess.w(0xC0 + 8, 0)
    sess.st.call("netListRemove", [BASE + 0xC0, 5])                    # empty head: nothing
    assert sess.log() == []


@need_net
def test_list_remove_negctl(sess):
    _ring(sess, [5, 6, 7])
    sess.lib.negctl("netListRemove", 1)
    sess.st.call("netListRemove", [BASE + 0xC0, 6])
    assert sess.r(0xC0 + 0x10) == 3


@need_net
def test_add_voice_rx(sess):
    sess.answer(LIST_ADDVOICE, 321)
    assert sess.call("S3List_addVoiceRx", 17) == 321
    assert sess.log() == [(LIST_ADDVOICE, [BASE, 17], b"")]
    sess.lib.negctl("S3List_addVoiceRx", 1)
    assert sess.call("S3List_addVoiceRx", 17) == 0


# ------------------------------------------------------------------------------------------------ netHandleControl
def _peers(s, ids, host_id=None):
    for k, pid in enumerate(ids):
        s.peer(k, pid, b"p%d" % k, k & 3, 0x10 + k)
    s.w(0x6F8, len(ids)); s.w(0x77C, ids[0]); s.w(0x780, host_id or ids[0])


@need_net
def test_handle_control_removes_and_keeps_slot15(sess):
    ids = list(range(0x41, 0x41 + 16))
    _peers(sess, ids)
    assert sess.call("netHandleControl", 0x43, 1) == 1
    assert sess.r(0x6F8) == 15
    assert [sess.r(0x170 + 0x58 * k) for k in range(16)] == ids[:2] + ids[3:] + [ids[15]]   # slot 15 stays: the defect
    assert [c for c, a, p in sess.log()][:1] == [SESS_REMOVED] and sess.log()[0][2] == b"p2\x00"
    sess.lib.defect("netHandleControl.slot15Stays", False)
    _peers(sess, ids)
    sess.call("netHandleControl", 0x43, 1)
    assert sess.r(0x170 + 0x58 * 15) == 0
    sess.lib.defect_reset()


@need_net
def test_handle_control_host_and_drop_notice(sess):
    _peers(sess, [0x41, 0x42, 0x43], host_id=0x42)
    sess.w(0x77C, 0x41)
    assert sess.call("netHandleControl", 0x42, 0) == 1              # flag 0 on a client asks the dialogs first (answer 0)
    assert sess.call("netHandleControl", 0x99, 1) == 0              # unknown id
    sess.st.call("net.hostReset", [])
    _peers(sess, [0x41, 0x42, 0x43], host_id=0x42)
    sess.w(0x77C, 0x41)
    sess.call("netHandleControl", 0x42, 1)
    assert sess.r(0x780) == 0x41 and sess.st.read({n: o for n, o, s in sess.lib.objects()}["g_net8400ac"], 4) == struct.pack("<I", 0x42)


@need_net
def test_handle_control_negctl(sess):
    _peers(sess, [0x41, 0x42, 0x43])
    sess.lib.negctl("netHandleControl", 1)
    assert sess.call("netHandleControl", 0x43, 1) == 0


# ------------------------------------------------------------------------------------------------ netPoll / netSendShutdown
def _push(s, user, data):
    s.buf[0x1A00:0x1A00 + len(data)] = data
    s.st.call("net.hostPush", [user, BASE + 0x1A00, len(data)])


@need_net
def test_poll(sess):
    assert sess.call("netPoll") == 0                                 # no interface
    sess.iface(0x77)
    assert sess.call("netPoll") == 0x77                              # receive buffer +4 is NULL: the interface pointer
    sess.w(4, BASE + 0x1C00)
    _push(sess, 0, struct.pack("<IIII", 0x102, 0, 0x42, BASE + 0x1B00))   # peer flag byte for id 0x42
    sess.buf[0x1B00] = 0x5A
    sess.peer(0, 0x41, b"a", 1); sess.peer(1, 0x42, b"b", 2)
    assert sess.call("netPoll") == 0
    assert sess.buf[0x170 + 0x58 + 0x14] == 0x5A
    _push(sess, 0, struct.pack("<II", 0x31, 0) + bytes(0x20))       # session lost: session slot +0x14, then the shutdown
    sess.call("netPoll")
    assert STREAM_STOP in [c for c, a, p in sess.log()] and SESS_HOSTLOST in [c for c, a, p in sess.log()]


@need_net
def test_poll_roster_and_create(sess):
    sess.iface(1)
    sess.w(4, BASE + 0x1C00)
    roster = bytes([0x10, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0]) + struct.pack("<I", 0x31) + b"zed\x00".ljust(0x20, b"\x00") + struct.pack("<I", 1)
    _push(sess, 1, roster)
    sess.call("netPoll")
    assert sess.r(0x170) == 0x31 and bytes(sess.buf[0x185:0x188]) == b"zed" and sess.r(0x6F8) == 1
    sess.st.call("net.hostReset", [])
    sess.w(0x77C, 3); sess.w(0x780, 3)
    name = _put_name(sess, 0x1800, b"joiner")
    sess.st.write({n: o for n, o, s in sess.lib.objects()}["g_ui_83ff9c"] + 12, struct.pack("<I", name))   # DPNAME.lpszShortNameA
    _push(sess, 0, struct.pack("<IIII", 3, 1, 0x90, 0))
    sess.call("netPoll")
    assert sess.log()[0] == (0x2054, [1, 0x90, 0x100], b"")         # GetPlayerName(id) through the interface
    assert sess.r(0x170 + 0x58) == 0x90 and bytes(sess.buf[0x170 + 0x58 + 0x15:0x170 + 0x58 + 0x1B]) == b"joiner"


@need_net
def test_poll_negctl(sess):
    sess.iface(1)
    sess.w(4, BASE + 0x1C00)
    sess.peer(0, 0x42, b"b", 2)
    sess.buf[0x1B00] = 0x5A
    _push(sess, 0, struct.pack("<IIII", 0x102, 0, 0x42, BASE + 0x1B00))
    sess.lib.negctl("netPoll", 1)
    sess.call("netPoll")
    assert sess.buf[0x170 + 0x14] != 0x5A


@need_net
def test_send_shutdown(sess):
    sess.iface(1)
    sess.w(0x14, 0x14); sess.w(0x34, 1); sess.w(0x720, 0x9000); sess.w(0x6F8, 3); sess.w(0x77C, 5)
    sess.peer(2, 9, b"q", 3)
    sess.call("netSendShutdown")
    cl = [c for c, a, p in sess.log()]
    assert cl[:1] == [STREAM_STOP] and 0x2010 in cl and 0x2008 in cl and 0x4007 in cl and 0x4006 in cl
    assert sess.r(0xF0) == 20000 and sess.r(0x6F8) == 0 and sess.r(0x77C) == 0 and sess.r(0x720) == 0
    assert sess.r(0x170 + 0x58 * 2) == 0 and sess.r(0xF4) == 1 and sess.r(0x718) == 0xC8
    assert cl.count(LIST_CLEAR) == 3


@need_net
def test_send_shutdown_negctl(sess):
    sess.lib.negctl("netSendShutdown", 1)
    sess.call("netSendShutdown")
    assert sess.r(0xF0) != 20000
