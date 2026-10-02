# Terrain.dll RVA 0x00001d50 Terrain::tileAt (live copy)

Module: `Terrain.dll` (anchor in `re/anchors.json`), image base `0x10000000`, so this body is VA
`0x10001d50`. Export `?tileAt@Terrain@@QAEPAVTile@@HH@Z` (RVA `0x108c`) is an incremental-linking
thunk `jmp 0x10001d50` (`0x1000108c`). `Terrain.dll` is a **debug** build (stack fill `0xcccccccc`,
`rep stosd` at `0x10001d67`; incremental-linking thunk table at `0x1000108c`..).

## Purpose
Returns the tile record at grid column `x`, row `y`, or NULL when either coordinate is outside the grid.
Bounds: `this+0x14` (`0x10001d73`) and `this+0x18` (`0x10001d84`); record size `0x248`
(`0x10001da2`); records start at `this+0x3a4` (`0x10001dab`). Same computation as the dead copy at
`0x004490d0` in `golf_clean.exe` (`re/analysis/terrain/004490d0_tileAt.md`). This copy runs:
22112 calls in 20 s of the sandbox scenario (record run 2026-10-02).

## Signature
`Tile* __thiscall Terrain::tileAt(Terrain* this, int x, int y)`: `this` in `ecx`, saved to
`[ebp-4]` (`0x10001d6a`); `x` = `[ebp+8]`, `y` = `[ebp+0xc]`; callee pops 8 (`ret 8` at `0x10001db8`).

## Reads
- `this+0x14` int32: `cmp ecx,[eax+0x14]; jge` (`0x10001d73`/`0x10001d76`), signed; also the stride
  (`imul edx,[ecx+0x14]` at `0x10001d99`).
- `this+0x18` int32: `cmp eax,[edx+0x18]; jge` (`0x10001d84`/`0x10001d87`), signed.
- `x < 0` -> NULL (`cmp [ebp+8],0; jl` at `0x10001d78`); `y < 0` -> NULL (`cmp [ebp+0xc],0; jge` at
  `0x10001d89`/`0x10001d8d`, falls to `xor eax,eax` at `0x10001d8f`).

## Writes
Stack frame only (`0x44` bytes filled with `0xcccccccc`, `0x10001d5a`..`0x10001d67`). No heap or global writes
(confirmed on 2000 recorded calls: emulated write sets outside the stack are empty for both bodies).

## Callers
11 functions reference the body or its thunk (Ghidra `Callers.java`). 8 are bodies of named exports
(name via export -> `jmp` thunk -> body, `re/terrain_exports_bodies.json`):
`Terrain::render` (`0x10005990`), `Terrain::pathUpdateRender` (`0x10006410`),
`Terrain::localRender` (`0x100089e0`), `Terrain::stripRender` (`0x10009270`),
`Terrain::hasConnectedPath` (`0x1000a450`), `Terrain::updatePath` (`0x1000a4b0`),
`Terrain::calcAllNormals` (`0x1000a740`), `Terrain::tileHit` (`0x1000ab30`).
Unnamed: `0x10007380`, `0x1000a130`, `0x10038900`.

## Callees
None (leaf).

## Constants
- `0x14` = 20, width offset (`0x10001d73`). `0x18` = 24, height offset (`0x10001d84`).
- `0x248` = 584, record size (`imul eax,eax,0x248` at `0x10001da2`).
- `0x3a4` = 932, offset of record 0 (`lea eax,[ecx+eax+0x3a4]` at `0x10001dab`).
- `0x11` = 17 dwords of `0xcccccccc` (`0x10001d5d`/`0x10001d62`): debug stack fill, 0x44 bytes.

## Verification
- Replay: `py -3.12 re/frida/replay.py TerrainDll_tileAt` -> `log/diff/00001d50_TerrainDll_tileAt.replay.csv`:
  2000 real calls (every 7th of 22112, sandbox Monterey, 20 s), live == emulated original == reimpl
  on every call, identical non-stack writes, identical stack cleanup. Inputs span x, y in `-1..50`;
  40 of 2000 calls take the NULL path.
- Path-1 A/B (extremes INT_MIN/INT_MAX the game never produced): same `TileAt_re` body, GREEN at
  `0x004490d0` (`log/diff/004490d0_Terrain_tileAt.path1.csv`).
- Install witness: `0xE9` at `0x10001d50` with the hook ON (record run, second boot).
