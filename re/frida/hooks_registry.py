"""Single source of truth for A/B test vectors. Add an entry here; never write a one-off harness.

Each entry drives re/frida/diff_hook.py:
  addr      VA (golf_clean.exe) the SG_HOOK is registered at
  abi       Frida ABI name: "thiscall" | "stdcall" | "default" (cdecl) | "fastcall"
  ret/args  Frida NativeFunction types
  fixture   name of a JS fixture in js/diff_fixtures.js; it returns {obj, ...} pointers that vectors
            reference as "$obj" (and pointer returns are reported relative to $obj, so arms compare)
  vectors   list of argument tuples
"""

_GRID = [(x, y) for x in (-1, 0, 1, 5, 9, 10, 11, 0x7FFFFFFF, -0x80000000) for y in (-1, 0, 3, 7, 8, 9)]

HOOKS = {
    "Terrain_tileAt": dict(
        module="golf_clean.exe",
        addr=0x004490D0,
        abi="thiscall",
        ret="pointer",
        args=["pointer", "int", "int"],
        fixture="terrain_10x8",
        vectors=[("$obj", x, y) for x, y in _GRID],
        note="re/analysis/terrain/004490d0_tileAt.md",
    ),
    # Live copy in Terrain.dll (RVA of the body; module-relative). Recorded/replayed by record.py/replay.py.
    "TerrainDll_tileAt": dict(
        module="Terrain.dll",
        addr=0x00001D50,
        abi="thiscall",
        ret="pointer",
        args=["pointer", "int", "int"],
        nargs=2,  # stack args (this is in ecx)
        fixture="terrain_10x8",
        vectors=[("$obj", x, y) for x, y in _GRID],
    ),
}

_EDGE = [-0x80000000, -0x7FFFFFFF, -100000, -129, -128, -65, -64, -63, -17, -16, -9, -8, -7, -2, -1, 0, 1, 2, 7, 8, 9,
         15, 16, 17, 63, 64, 65, 127, 128, 129, 1000, 100000, 0x7FFFFFFE, 0x7FFFFFFF]
_SMALL = [-20, -11, -10, -6, -5, -1, 0, 1, 2, 4, 5, 6, 8, 9, 10, 15, 16, 17, 18, 19, 20, 0x7FFFFFFF, -0x80000000]
_PAIRS = [(a, b) for a in (-0x80000000, -1000, -3, -1, 0, 1, 2, 3, 7, 1000, 0x3FFFFFFF, 0x7FFFFFFF)
          for b in (-0x80000000, -1000, -4, -1, 0, 1, 4, 8, 999, 1001, 0x3FFFFFFF, 0x7FFFFFFF)]
_DECAY2 = [(x, y) for x in (-100000, -64, -1, 0, 1, 15, 16, 64, 1000, 100000, 0x7FFFFFFF)
           for y in (-0x80000000, -1000, -16, -1, 0, 1, 15, 16, 127, 128, 129, 1000, 100000)]
_TILES = [(x, y) for x in (-0x80000000, -1, 0, 1, 2, 9, 24, 25, 48, 49, 50, 51, 0x7FFFFFFF)
          for y in (-0x80000000, -1, 0, 1, 2, 3, 13, 26, 48, 49, 50, 0x7FFFFFFF)]
_TILES += [(x, y) for x in range(50) for y in range(50) if (x * 7 + y * 3) & 0x1F in (0x13, 0x14, 0x15)]

HOOKS.update({
    # Batch 1 of C3 (2026-10-06): __cdecl integer leaves, hand-written in shim/src/re/golf_math.cpp.
    "approxDistance": dict(module="golf_clean.exe", addr=0x00467170, abi="default", ret="int", args=["int", "int"],
                           vectors=_PAIRS),
    "bucketValue": dict(module="golf_clean.exe", addr=0x0044FAF0, abi="default", ret="int", args=["int"],
                        vectors=[(v,) for v in sorted(set(_SMALL + _EDGE))]),
    "decaySum": dict(module="golf_clean.exe", addr=0x004223C0, abi="default", ret="int", args=["int"],
                     vectors=[(v,) for v in _EDGE]),
    "decaySum2": dict(module="golf_clean.exe", addr=0x004223F0, abi="default", ret="int", args=["int", "int"],
                      vectors=_DECAY2),
    "scaleX": dict(module="golf_clean.exe", addr=0x00404970, abi="default", ret="int", args=["int"],
                   vectors=[(v,) for v in _EDGE + [160, 319, 320, 321, 640, 799, 800]]),
    "tierPrice": dict(module="golf_clean.exe", addr=0x0046F1D0, abi="default", ret="int", args=["int"],
                      vectors=[(v,) for v in range(-3, 20)] + [(-0x80000000,), (0x7FFFFFFF,), (0x10,), (0xFFFF,)]),
    "tileBlocked": dict(module="golf_clean.exe", addr=0x0040BF60, abi="default", ret="int", args=["int", "int"],
                        fixture="tile_types_pattern", vectors=_TILES),
})

_CELLS = [(x, y) for x in (0, 1, 2, 10, 24, 25, 47, 48, 49) for y in (0, 1, 2, 13, 26, 47, 48, 49)]
HOOKS.update({
    # Batch 2 of C3 (2026-10-06): read-only table lookups, shim/src/re/golf_tables.cpp, fixture golf_tables.
    "tileType": dict(module="golf_clean.exe", addr=0x004492D0, abi="default", ret="int", args=["int", "int"],
                     fixture="golf_tables", vectors=_CELLS),
    "wallHeight": dict(module="golf_clean.exe", addr=0x00449330, abi="default", ret="int", args=["int", "int", "int"],
                       fixture="golf_tables",
                       vectors=[(x, y, d) for x in (1, 2, 10, 25, 48) for y in (1, 3, 13, 26, 48) for d in range(8)]),
    "cellAt": dict(module="golf_clean.exe", addr=0x0043D6F0, abi="default", ret="pointer", args=["int", "int", "int"],
                   fixture="golf_tables",
                   vectors=[(t, c, r) for t in range(4) for c in (-0x80000000, -2, -1, 0, 1, 2, 4, 5, 6, 7, 0x7FFFFFFF)
                            for r in (0, 1, 3)]),
    "golferScore": dict(module="golf_clean.exe", addr=0x00453260, abi="default", ret="int", args=["int"],
                        fixture="golf_tables", vectors=[(g,) for g in range(16)]),
    "typeBit7Clear": dict(module="golf_clean.exe", addr=0x0046C940, abi="default", ret="int", args=["int"],
                          fixture="golf_tables", vectors=[(g,) for g in range(16)]),
    "pointInRect": dict(module="golf_clean.exe", addr=0x00492610, abi="default", ret="int",
                        args=["int", "int", "pointer"], fixture="rect_10_20_30_40",
                        vectors=[(x, y, "$obj") for x in (-0x80000000, 0, 9, 10, 11, 29, 30, 31, 0x7FFFFFFF)
                                 for y in (-0x80000000, 0, 19, 20, 21, 39, 40, 41, 0x7FFFFFFF)]),
})

_CLEAR = [(x, y) for x in (0, 1, 7, 24, 48, 49) for y in (0, 2, 13, 25, 48, 49)]
_COURSE_STATE = [(0x005722E8, 0, 2500), (0x0053CAF0, 0, 5000), (0x0059BF90, 0, 4), (0x00571FF7, 0, 1)]
HOOKS.update({
    # Stateful functions (2026-10-06): diff_hook snapshots the `state` regions, runs both arms from the same
    # snapshot, compares return value AND the regions afterwards, then restores them. shim/src/re/golf_state.cpp.
    "Random_next": dict(module="golf_clean.exe", addr=0x0045C1A0, abi="thiscall", ret="double", args=["pointer"],
                        fixture="rng_seeds", state=[("$obj", 0, 64)],
                        vectors=[(f"$s{i}",) for i in range(16)] * 2),
    "clearTile": dict(module="golf_clean.exe", addr=0x00470A10, abi="default", ret="void", args=["int", "int"],
                      fixture="course_kind2", state=_COURSE_STATE, vectors=_CLEAR),
    "clearTile_kind0": dict(module="golf_clean.exe", addr=0x00470A10, abi="default", ret="void", args=["int", "int"],
                            fixture="course_kind0", state=_COURSE_STATE, vectors=_CLEAR),
})

_MSG_STATE = [(0x0053BBA8, 0, 4), (0x0056C770, 0, 32), (0x0056C794, 0, 32), (0x0056A794, 0, 32), (0x0056A924, 0, 32),
              (0x0056C570, 0, 512)]
_POP_STATE = [(0x0059ABB0, 0, 4), (0x00542FD8, 0, 32), (0x00542FF8, 0, 32), (0x00542DD8, 0, 32), (0x00542F00, 0, 32),
              (0x00575CA0, 0, 4 * 0x208)]
_POPS = [(p, x, y, w) for p in (0, 5, -3, 0x7FFFFFFF) for x, y in ((0, 0), (17, -4)) for w in (-1, 0, 3)]
HOOKS.update({
    # Batch 3 of C3 (2026-10-06): writers, shim/src/re/golf_writers.cpp, A/B'd with state regions.
    "queueMessage": dict(module="golf_clean.exe", addr=0x0040C720, abi="default", ret="void", args=["int", "int", "int"],
                         fixture="msg_ring_n7", state=_MSG_STATE,
                         vectors=[(a, b, c) for a in (0, 1, -1) for b in (0, 7) for c in (0, 0x7FFFFFFF)]),
    "queueMessage_n3": dict(module="golf_clean.exe", addr=0x0040C720, abi="default", ret="void",
                            args=["int", "int", "int"], fixture="msg_ring_n3", state=_MSG_STATE,
                            vectors=[(a, b, c) for a in (0, 2) for b in (1, -5) for c in (3,)]),
    "clearMatching": dict(module="golf_clean.exe", addr=0x0040C860, abi="default", ret="void", args=["int", "int"],
                          fixture="msg_ring_n7", state=_MSG_STATE,
                          vectors=[(a, b) for a in (-1, 0, 1, 2, 3) for b in (-1, 0, 1, 2)]),
    "pointsPopup": dict(module="golf_clean.exe", addr=0x0040C890, abi="default", ret="void",
                        args=["int", "int", "int", "int"], fixture="pop_ring_n7", state=_POP_STATE, vectors=_POPS),
    "pointsPopup_n2": dict(module="golf_clean.exe", addr=0x0040C890, abi="default", ret="void",
                           args=["int", "int", "int", "int"], fixture="pop_ring_n2", state=_POP_STATE, vectors=_POPS),
    "pointsPopup_off": dict(module="golf_clean.exe", addr=0x0040C890, abi="default", ret="void",
                            args=["int", "int", "int", "int"], fixture="pop_ring_off", state=_POP_STATE, vectors=_POPS),
    "logTick": dict(module="golf_clean.exe", addr=0x0040C6F0, abi="default", ret="void", args=["int", "int"],
                    fixture="tick_12345", state=[(0x00568600, 0, 1000), (0x00834170, 0, 4)],
                    vectors=[(a, b) for a in (0, 1, 0x7FFF, 0x12345678, -1) for b in (0, 2, 0x8000)]),
    "resetGolfer": dict(module="golf_clean.exe", addr=0x00426670, abi="default", ret="void", args=["int"],
                        fixture="golfer_queue", state=[(0x005794D5, 0, 0x1000), (0x005689E8, 0, 800)],
                        vectors=[(g,) for g in range(16)]),
    "appendOpinion": dict(module="golf_clean.exe", addr=0x00469A20, abi="default", ret="void", args=["int", "int"],
                          fixture="text_ab", state=[(0x0051A068, 0, 1024)],
                          vectors=[(r, t) for r in (-0x80000000, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x7F, 0x80, 0x7FFFFFFF)
                                   for t in (-1, 0, 1, 2)]),
})

# world coordinates: tile = v >> 10; offsets around the 0x200 half-tile split and the tile edges
_WORLD = [t * 1024 + o for t in (0, 1, 3, 7, 12, 25, 31, 48) for o in (0, 1, 0x1FF, 0x200, 0x201, 0x3FF)]
_WXY = [(x, y) for i, x in enumerate(_WORLD) for y in _WORLD[i % 7::7]]
HOOKS.update({
    # Batch 4 of C3 (2026-10-06): terrain height/slope readers, shim/src/re/golf_terrain.cpp, fixture terrain_slopes.
    "slopeX": dict(module="golf_clean.exe", addr=0x0040C3A0, abi="default", ret="int", args=["int", "int"],
                   fixture="terrain_slopes", vectors=_WXY),
    "slopeY": dict(module="golf_clean.exe", addr=0x0040C2F0, abi="default", ret="int", args=["int", "int"],
                   fixture="terrain_slopes", vectors=_WXY),
    "slopeMix": dict(module="golf_clean.exe", addr=0x0040C450, abi="default", ret="int", args=["int", "int", "uint"],
                     fixture="terrain_slopes", vectors=[(x, y, d) for x, y in _WXY[::3] for d in range(8)]),
    "heightAt42fa30": dict(module="golf_clean.exe", addr=0x0042FA30, abi="default", ret="int", args=["int", "int"],
                           fixture="terrain_slopes", vectors=_WXY),
})

_BLEND = [(x, y) for x in (-1, 0, 1, 2, 7, 13, 24, 25, 31, 48, 49, 50) for y in (-1, 0, 1, 5, 12, 15, 16, 17, 30, 48, 49, 50)]
_PLACE = [(x, y, n, k) for x, y in ((1, 1), (3, 7), (10, 20), (20, 5), (31, 33), (44, 40))
          for n in (-2, -1, 0, 1, 2, 3) for k in (0, 3, 4, 7, 12)]
HOOKS.update({
    # Batch 5 of C3 (2026-10-06): course/golfer table readers, shim/src/re/golf_course.cpp.
    "rateTile": dict(module="golf_clean.exe", addr=0x00422530, abi="default", ret="int", args=["int"],
                     fixture="ratings_mode0", vectors=[(i,) for i in range(0xa0)]),
    "rateTile_mode1": dict(module="golf_clean.exe", addr=0x00422530, abi="default", ret="int", args=["int"],
                           fixture="ratings_mode1", vectors=[(i,) for i in range(0xa0)]),
    "heightBlend": dict(module="golf_clean.exe", addr=0x0040C170, abi="default", ret="int", args=["int", "int"],
                        fixture="blend_c2", vectors=_BLEND),
    "heightBlend_c0": dict(module="golf_clean.exe", addr=0x0040C170, abi="default", ret="int", args=["int", "int"],
                           fixture="blend_c0", vectors=_BLEND),
    "heightBlend_tick": dict(module="golf_clean.exe", addr=0x0040C170, abi="default", ret="int", args=["int", "int"],
                             fixture="blend_tick", vectors=_BLEND),
    "evalPlacementArea": dict(module="golf_clean.exe", addr=0x0040DB90, abi="default", ret="int",
                              args=["int", "int", "int", "int"], fixture="place_ct0", vectors=_PLACE),
    "evalPlacementArea_ct1": dict(module="golf_clean.exe", addr=0x0040DB90, abi="default", ret="int",
                                  args=["int", "int", "int", "int"], fixture="place_ct1", vectors=_PLACE),
})


# Fragments (one file per C3 batch, so parallel agents never edit the same file): re/frida/registry.d/<batch>.py
# each defines HOOKS = {name: dict(...)} in the format above; names must be unique across all files.
def enabled_batches():
    """Batch ids listed in shim/re_batches.txt (the ones linked into the shim and verified by diff_hook)."""
    import pathlib
    f = pathlib.Path(__file__).resolve().parents[2] / "shim" / "re_batches.txt"
    if not f.exists():
        return set()
    return {l.strip()[:-4] for l in f.read_text().splitlines() if l.strip().endswith(".cpp") and not l.startswith("#")}


def _load_fragments():
    import importlib.util, pathlib, sys
    enabled = enabled_batches()
    for f in sorted((pathlib.Path(__file__).parent / "registry.d").glob("*.py")):
        spec = importlib.util.spec_from_file_location(f"registry_d_{f.stem}", f)
        mod = importlib.util.module_from_spec(spec)
        try:
            spec.loader.exec_module(mod)
        except Exception as e:                     # a batch still being written must not break verification
            if f.stem in enabled:
                raise
            print(f"hooks_registry: skipped {f.name} (not enabled, {type(e).__name__}: {e})", file=sys.stderr)
            continue
        dup = set(mod.HOOKS) & set(HOOKS)
        if dup:
            raise ValueError(f"{f.name}: hook names already registered: {sorted(dup)}")
        HOOKS.update(mod.HOOKS)


_load_fragments()
