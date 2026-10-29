"""S4.9b keys of loadSoundFile (rva 0x41d0; __thiscall (this, name), `ret 4` at 0x507f): registry-shaped specs whose fixtures are
log/roadmap/S4/9b_fixtures.js's F.s49b_<key>. Imported by 9b_replay.py. Vectors are (Sound object, file name) pairs.

State regions: every Sound object, the heap stand-in arena (+0x10 header: cursor, call counter, fail mask) and its free log, the mmio
stand-in block (files, handles, op log) and its import log, the stub log, the swapped module words (g_openMmioCount rva 0xb4a08, the
sound list rva 0xb4fe8 .. +0xc and its fixture nodes / entries, the string at rva 0x69988) and the +0x218 range lists' nodes and records.
"""

ADDR = 0x41d0
SOUND_SIZE = 0xB00          # 9b_fixtures.js SOUND_SIZE
ARENA = 0x40000             # 9b_fixtures.js lsf(): o.arena default
MMSIZE = 0x20000            # 9b_fixtures.js MMSIZE


def _state(n, nlist):
    s = [("$objs", 0, n * SOUND_SIZE), ("$arena", 0, 0x10 + ARENA), ("$freelog", 0, 0x404), ("$mmblock", 0, MMSIZE),
         ("$implog", 0, 0x4000), ("$log", 0, 0x400), ("$glist", 0, 0x10), ("$gmmio", 0, 4), ("$lnodes", 0, 0x400),
         ("$gstr", 0, 0x40), ("$rnodes", 0, 0x40 * n * 8), ("$recs", 0, 0x20 * n * 8)]
    s += [(f"$lent{i}", 0, 0x220) for i in range(nlist)]
    return s


def _key(fixture, n, nlist=0, vectors=None):
    return dict(module="sound.dll", addr=ADDR, abi="thiscall", ret="uint32", args=["pointer", "pointer"], fixture=fixture,
                state=_state(n, nlist), vectors=vectors if vectors is not None else [(f"$s{i}", f"$n{i}") for i in range(n)])


CUSTOM9B = {
    "loadSoundFile@basic": _key("s49b_basic", 11, 1),
    "loadSoundFile@busy7": _key("s49b_busy7", 2),
    "loadSoundFile@busy0": _key("s49b_busy0", 2),
    "loadSoundFile@seekfail": _key("s49b_seekfail", 4),
    "loadSoundFile@seeknomove": _key("s49b_seeknomove", 3),
    "loadSoundFile@nopre": _key("s49b_nopre", 2),
    "loadSoundFile@list": _key("s49b_list", 2, 3),
    "loadSoundFile@pre_g0": _key("s49b_pre_g0", 5),
    "loadSoundFile@pre_g1": _key("s49b_pre_g1", 5),
    "loadSoundFile@pre_g2": _key("s49b_pre_g2", 5),
    "loadSoundFile@pre_g3": _key("s49b_pre_g3", 5),
    "loadSoundFile@pre_g4": _key("s49b_pre_g4", 5),
    "loadSoundFile@pre_g5": _key("s49b_pre_g5", 5),
    "loadSoundFile@oom0": _key("s49b_oom0", 2),
    "loadSoundFile@oom1": _key("s49b_oom1", 1),
    "loadSoundFile@oom2": _key("s49b_oom2", 1),
    "loadSoundFile@nullA": _key("s49b_nullA", 4),
    "loadSoundFile@nullB": _key("s49b_nullB", 1),
    "loadSoundFile@nullC": _key("s49b_nullC", 1),
    "loadSoundFile@nullU": _key("s49b_nullU", 1),
    "loadSoundFile@sfnull": _key("s49b_sfnull", 1),
    "loadSoundFile@sfnullU": _key("s49b_sfnullU", 1),
    "loadSoundFile@snmnull": _key("s49b_snmnull", 1),
    "loadSoundFile@snmnullU": _key("s49b_snmnullU", 1),
    "loadSoundFile@user": _key("s49b_user", 3),
}

# the user's own files the `user` key loads (paths relative to original\; 9b_replay.py reads them and hands them to the fixture)
USER_FILES = ("Sounds/Interface/Text Box.wav", "Sounds/vssver.scc", "Sounds/Effects/Sheep 2.wav")

# development key (not part of the gate): one chunk of each layout before a MIDI image
DEV9B = {"loadSoundFile@dbg": _key("s49b_dbg", 12)}
