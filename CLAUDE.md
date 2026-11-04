# CLAUDE.md — SimGolf RE

Reverse engineering of **Sid Meier's SimGolf** (Firaxis / Maxis / EA, 2002), v1.03, built on the
Mashed RE workflow (`..\Mashed\CLAUDE.md`): Ghidra + Frida, NO-GUESSING, the C0..C4 confidence
ladder, signature-checked patches, and a test suite that boots the real game.

**Shape of the project: hook-based, not greenfield.** The stock game runs; our code lives in a shim
DLL that fixes Windows 11 compatibility, applies patches, and hosts reimplementations that are A/B
tested against the original inside the live process.

## Anchors (verify before citing any address)

All in `re/anchors.json`, enforced by `tests/test_static.py`.

```
original\golf_clean.exe  2,286,953  E74EA9F95350796432CB1CCDA3BFFE00E48789F3E410CD5FFB61F70ACEB26789   <- THE TARGET
original\golf.exe        2,286,953  200711A655DEB60DD63B2C379B25086B90CBF9A0B8C8BCC5427B224201BAC0A2   (SafeDisc 2.60.52, v1.03)
original\jgld.dll        1,273,898  07D6B89C9CAE741222DF7741C9116ACE5F64EC7B7B44124092AB7BC1C86629D4   (Jackal gfx, DEBUG build; the one loaded)
```

`golf_clean.exe` = `golf.exe` v1.03 unwrapped by Safedisc2Cleaner. Image base `0x00400000`, no
relocations, so addresses are VAs. DLL addresses are RVAs (the `module` column says which).
**Never modify `golf_clean.exe` on disk**: it is the A/B reference. Patches go through the shim.

## Engine and binaries

- **Jackal** engine (Firaxis; Alpha Centauri lineage: `jackal.txt` still has SMAC error strings).
- `jgld.dll` — 2D GDI/DIB graphics, window, fonts (debug build, `C:\JackDev\Debug\jgld.pdb`; U-0001).
- `Terrain.dll` — OpenGL 3D terrain; exports the `Terrain` class (golf imports 26 methods).
- `sound.dll` — DirectSound/winmm. `binkw32.dll` — RAD Bink 1.x video.
- `golf_clean.exe` exports 4 Terrain accessors (`tileAt`, `getElevation`, `getWall`, `getType`) that
  are **dead code** (0 runtime calls); the live ones are in `Terrain.dll`.
- Compiler: linker 6.0 (MSVC 6) for all three. `Terrain.dll` and `jgld.dll` are **debug** builds
  (stack fill `0xcccccccc`, incremental-linking `jmp` thunks: exports point at thunks, not bodies).
- `golf_v100_clean.exe` (retail v1.00, SafeDisc 2.51.021 unwrapped) is in the Ghidra master for version
  diffing only: `re/versions/PATCH_MAP.md` links v1.01-1.03 patch notes to changed functions.

## Runtime (how it runs on Win11) — details in `re/analysis/PORTABLE_RUNTIME.md`

`Play SimGolf.bat` -> `original\golf_clean.exe` + `original\winmm.dll` (our shim, a winmm proxy).
No installer, no registry (no game module imports ADVAPI32), no SafeDisc, windowed 800x600.

Shim (`shim/`, build `shim\build.bat`, MSVC x86 + vendored MinHook). Config `original\simgolf_shim.ini`,
env overrides `SIMGOLF_*`, log `original\simgolf_shim.log`:
- `windowed=1` — suppress `ChangeDisplaySettingsA`, fake 800x600 screen metrics, framed window, no `ClipCursor`.
- `bink_dib=1` — force `BINKBUFFERDIBSECTION`; `BINKBUFFERAUTO` crashes inside `BinkBufferOpen` on Win11.
- `skip_intro=0` — tests set `SIMGOLF_SKIP_INTRO=1` (menu in ~4.5 s).
- `patches/*.ini` — signature-checked byte patches (`expect` must match or the patch is refused).
- `shim/src/re/*.cpp` — reimplementations, `SG_HOOK(module, addr, name, detour, orig)`, toggle with
  `[hooks] <addr>=0` or `SIMGOLF_HOOKS_OFF=<addr>,...|all`.
- Test exports: `SimGolfShim_SetVirtualCursor`, `SimGolfShim_FindHook`, `SimGolfShim_GetInfo`,
  `SimGolfShim_CovPhase`/`CovDump` (INT3 function coverage, `SIMGOLF_COVERAGE=<entries file>`).
  `SIMGOLF_VCURSOR=x,y` makes the virtual cursor live from the first frame (the harness always sets it).
  `SimGolfShim_SetVirtualKey(vk, 0|1|2)` overlays GetKeyboardState/GetKeyState/GetAsyncKeyState: the
  in-game UI reads mouse buttons from GetKeyboardState (~13/s), which posted messages never update.
- `SIMGOLF_TIMEWARP=N` scales timeGetTime/GetTickCount/QueryPerformanceCounter (x32 + Esc every 2 s =
  ~1 game year per 40 s; event popups such as SimFoto hold game time until Esc clears them).

## Layout

```
SimGolf\
├── SM_SIMGOLF.iso, iso\, extracted\   # media and raw extraction (never edit)
├── original\                          # the portable install; golf_clean.exe + shim deployed here
├── shim\                              # winmm.dll proxy: compat fixes, patch loader, SG_HOOK registry
├── patches\                           # one .ini per byte patch
├── re\
│   ├── anchors.json, CONFIDENCE.md, functions_ghidra.tsv (2,934 exe fns), functions_ghidra_Terrain.dll.tsv (1,050)
│   ├── coverage\, versions\, replay\ (recordings, not committed), scoreboard.tsv
│   ├── analysis\<subsystem>\<addr>_<name>.md, analysis\CHANGELOG.md, analysis\PORTABLE_RUNTIME.md
│   ├── frida\  boot_trace.py, dump_image.py, diff_hook.py, hooks_registry.py, js\*.js
│   └── tools\  game.py (harness), imgcmp.py, decomp.py (headless Ghidra), winshot.py
├── tests\                             # pytest suite (see "Tests")
├── scripts\                           # re_classify.py (confidence gate), ghidra_pool.{sh,ps1}, ghidra_assert.sh
├── ghidra\scripts\                    # ExportFacts.java, ListFunctions.java, Decompile.java
├── SimGolf.gpr / SimGolf.rep          # Ghidra MASTER; simgolf_pool\ = read-only clones
├── tools\                             # unshield, SafeDiscLoader2, Safedisc2Cleaner driver (one-time setup)
└── hooks.csv, UNCERTAINTIES.md, STUBS.md, DEFERRED.md   # trackers
```

## Toolchain

- Ghidra 12.0.3 shared from `..\TD5RE\ghidra\ghidra_12.0.3_PUBLIC` (do not duplicate); MCP
  `ghidra-headless-mcp` from `..\TD5RE\ghidra\headless-mcp\` and `frida-game-hacking` — both wired in
  `.mcp.json` (same as Mashed; a new session picks them up).
- Python 3.12 (`py -3.12`): frida 17, pefile, capstone, pywinauto, pillow, numpy, pytest.
- MSVC Build Tools 2022 x86 (`vcvars32.bat`) for the shim.
- **Visual C++ 6** (cl 12.00.8168, the exact build in the game's Rich headers) in `tools/vc6/vc98`
  (extracted from the user's `Visual Studio 6.iso`, no installer; not committed). Used only by
  `re/tools/match.py` for matching decompilation.

### Matching decompilation (full reference: `re/history/MATCHING.md`)
Plan `re/match/PLAN.md`. `py -3.12 re/tools/match.py re/match/<file>.cpp` compiles with VC6 (`tools/vc6/vc98`, cl 8168;
`// TOOLCHAIN sp5` for sound.dll's C++), compares against the anchored binary and writes `log/diff/<addr>_<name>.match.csv`.
Sources carry `// MATCH: <module> <addr> <decorated name>`, `// FLAGS <module>: ...`, optional `// LANG c`, `// INCLUDE`,
`// COMPILE`. Flags: exe `/O2` (+`/GX` where an SEH frame exists), Terrain `/Od /ZI /GZ /GX /MTd`, jgld `/Od /ZI /GZ [/GX]`,
sound.dll three builds (C++ /O2, LPC-10 codec /Od C, CRT /O1). Bulk: `decomp.py` -> `ghidra2src.py` -> `g2s_collect.py`;
near misses `match_permute.py`; library code `match_autoname.py`; SMSG sources `smsg_import.py`. Extents Ghidra gets
wrong go in `re/match/boundaries.tsv`; misses in `re/match/wip/`. Never pass `--evidence` while exploring (a RED CSV breaks
the C4 gate of a row already at C4). In Git Bash use `MSYS_NO_PATHCONV=1` with `--flags "/O2"`. A naked inline-asm
transcription is NOT match evidence; library-object identity and an `__asm` block inside compiled C are (G26 / round 36).
### Ghidra project

Master `SimGolf.gpr` holds `golf_clean.exe` (2,207 functions after analysis), `jgld.dll`,
`Terrain.dll`, `sound.dll`. Rebuild recipe (~4 min):
```
<ghidra>\support\analyzeHeadless.bat C:\Users\maria\Desktop\Proyectos\SimGolf SimGolf ^
  -import original\golf_clean.exe original\jgld.dll original\Terrain.dll original\sound.dll
```
Use the `ghidra-pool` skill for slots. Without the MCP: `py -3.12 re\tools\decomp.py 0x<addr>`.

## Tests (`test-suite` skill)

```
py -3.12 -m pytest -m static                  # anchors, SafeDisc removal, portability, shim surface, gate unit tests
py -3.12 -m pytest -m "runtime and not slow"  # boots the game windowed; Frida; input; in-game; patches; A/B tool
py -3.12 -m pytest -m ghidra                  # master project, headless facts via a pool slot, MCP tools/list
py -3.12 -m pytest -m slow                    # 5x boot reliability, full intro through the Bink fix
py -3.12 -m pytest -m ""                      # FULL suite (pytest.ini addopts skips slow and ghidra by default)
```

## Workflow rules

### NO-GUESSING
Report only what the decompilation/disassembly literally shows; cite the address of every constant
and offset; raw hex + signed decimal for sign-sensitive values; unclear -> `[UNCERTAIN U-NNNN]`
filed in `UNCERTAINTIES.md`. Banned: *probably, likely, seems to, appears to, I think, presumably,
might be, maybe* (the gate rejects notes containing them). Verify claims you write: the first note
here claimed `Terrain.dll` called the exe's `tileAt`; the import table and a runtime counter said no.

### Confidence ladder (`re-classify` skill)
`re/CONFIDENCE.md` is the rubric; `scripts/re_classify.py` is the only writer of `hooks.csv` and
`re/analysis/CHANGELOG.md`. One level at a time. C3 needs a GREEN path-1 A/B with install witness and
an anti-island caller and callee. C4 needs a canonical-scenario ON/OFF run (D-0002). Path-1 is never
C4 evidence.
`hooks.csv` is keyed by (module, addr): the three DLLs share the image base 0x10000000 and 204 RVAs collide.
`re_classify.py batch <tsv...> --to C1` promotes every row of a names TSV through the same gates (DLL VAs are
converted to RVAs; one CHANGELOG line each); `status --summary` prints counts per module; `retag <tsv...>` copies a
TSV's subsystem column onto tracked rows (level unchanged). Subsystem `net` (Jackal network layer, exe 0x497b40-0x49bec0)
was added 2026-10-06. `re/functions_ghidra*.tsv` are exported from the master with
`-postScript ListFunctions.java <out.tsv> -readOnly` (namespaced names; thunks carry their target's name, no `thunk_`
prefix, so tools exclude them by size <= 5).

### Names and systems (`re/names/`, `re/analysis/systems/`)
`re/names/<id>.tsv` (`module addr name subsystem evidence purpose`) and `<id>_globals.tsv` (`module addr name type
evidence`) hold evidence-backed names. `terrain.tsv` and `libs.tsv` (libpng/zlib/libjpeg) come from matched sources
via `re/tools/names_from_match.py`; `exe_0..5`, `jgld_0..1`, `sound_0..1` were named by agents from
`log/naming_brief.md` (2026-10-06) with a system writeup each in `re/analysis/systems/<id>.md`. Before committing
new ones: `re/tools/names_sanitize.py` (quoted binary text -> placeholder; the repo is public) and
`re/tools/names_validate.py <id>...` (also checks names/addresses unique across all files of a module) and
`re/tools/names_check_evidence.py` (every `string 0x..` / `import X` citation must be referenced by the function;
it caught 2 round-1 sound.dll names whose cited string the function never touches). sound.dll round 2 (`sound_2..5`,
2026-10-06) took it from 127 to 471 named; the mixer dispatch (`count` vs 0xb) selects accumulate vs overwrite bodies
(verified: 0x10019770 and 0x10019b20 differ only by `fadd [edi]`). Withdrawn names go in `re/names/withdrawn.tsv`
(ApplyNames resets them to FUN_; `re_classify.py demote` lowers the row). `ghidra/scripts/ApplyNames.java <re/names dir>
[overwrite]` applies them to a program (USER names kept unless `overwrite`, IMPORTED always kept, `Class::method` ->
namespaces), run headless with `-process <prog> -noanalysis`.
`re/tools/xref.py <module> 0x<addr>` gives offline callers/callees/strings (capstone, thunks resolved, cached in log/).
`re/tools/c2_note.py` writes a C2 transcription per function (reads/writes with width, branch signedness, callees with
imports and resolved thunks, constants, convention from `ret N` and registers read before written; never a
`## Purpose`), linking the names row, the system writeup and any hand-written note; `--all` covers every C1 row.
C2 = this mechanical transcription (decided 2026-10-06): all 2669 named functions were promoted with
C3 method in brief (every round's detail: `re/history/C3_ROUNDS.md`; brief `log/c3_brief.md`). A batch is
`shim/src/re/<ID>.cpp` + `re/frida/registry.d/<ID>.py` + `re/frida/js/fixtures.d/<ID>.js` + `log/c3/<ID>_purpose.md`,
listed in `shim/re_batches.txt`; `shim\check_re.bat` compile-checks, `shim/build_instance.ps1 -Id <ID>` builds an instance,
`re/tools/c3_verify.py <ID> [--apply] [--callers a=b] [--callees a=list] [--skip ...]` gates and promotes. Shared fixture
helpers (load order in diff_hook.py): heapstandin.js, importstandin.js, filestandin.js, mmiostandin.js, freeze.js,
recdisplay.js, uitree.js, modal.js, framemask.js. Holds with a measured reason: `re/c3_hold.tsv`. Standing rules:
- Hand-written reimplementations, never the recompiled match. Purpose paragraphs are `**Purpose:**` and cite an
  instruction address; every address needs its own line starting `// 0x<addr>`.
- The gate sums every `log/diff/<addr>_*.path1.csv` of the row's module: delete the CSV of a removed key.
- Before building, scan every linked batch for duplicate SG_HOOK addresses; `re/tools/check_ret_args.py` checks
  `ret N` against registry arity. A void reimplementation of a function whose eax a caller returns is a defect.
- When a new batch hooks a callee of existing C3 functions, re-run those callers with the hooks on and off.
- Give every ERREXIT / error path a returning-handler key; check which branches a fixture reaches, not only the spread.
- One batch per boot; re-run a RED / ERROR once in a fresh boot before believing it; freezing fixtures use
  `__freeze`, never SuspendThread; negctl switches are exported `<Batch>NegCtl`, never a fixed-address page.
- Stand-in results must depend only on arguments or compared memory (no JS-side counters).
Function bodies are not always contiguous (86 functions before b1; mainLoop 0x40f5c0 is now one body ending at 0x421615).
`re/functions_ghidra*.tsv` carry a `ranges` column (ListFunctions.java) and xref.py / c2_note.py disassemble every
range; before that, calls in the far ranges were missed (clearMatching looked uncalled). After the index changes, run
`c2_note.py --refresh --out log/c2_refresh.tsv` + `re_classify.py retag log/c2_refresh.tsv` (rewrites generated notes
without a hand Purpose, refreshes callers/callees in hooks.csv).
`py -3.12 re/frida/diff_hook.py <names...>` runs all in one boot; then `re_classify.py promote <addr> --to C3 --file <cpp>`.
`xref.load_pe` is fast_load: call `pe.parse_data_directories()` before reading imports.

### A/B verification (`diff-original` skill)
Add vectors to `re/frida/hooks_registry.py`, run `py -3.12 re/frida/diff_hook.py <name>`. Never write
one-off harnesses; extend the registry. For live functions prefer **record/replay**:
`py -3.12 re/frida/record.py <name>` (real calls + memory, hook OFF) then `py -3.12 re/frida/replay.py <name>`
(Unicorn, offline: live == emulated original == reimpl). `re/tools/invariants.py <name>` infers contracts
from the recording that neighbours can be checked against.

### Scoreboard, coverage, version diff, failures
- `py -3.12 scripts/scoreboard.py` — Reach x Understanding x Verification per function; priority list.
- `py -3.12 re/tools/coverage_census.py --runs 3` — INT3 census of the sandbox scenario (exe + Terrain.dll).
- `py -3.12 re/tools/version_diff.py re/versions/v100.jsonl re/versions/v103.jsonl` — function matching.
- `re/tools/sbfl.py` (Ochiai), `re/tools/cov_bisect.py` (ddmin; needs a deterministic oracle).
- Read `re/analysis/TESTING_STRATEGY.md` before trusting any FAIL: first decide harness vs game.

### Function ID, saves, time travel
- **FidDb** `re/fid/simgolf_vc6.fidb` (not committed; `py -3.12 re/tools/build_fiddb.py`, ~45 min): the 12 VC6
  static CRT/STL/iostream libs (release + debug) + `original\JPEG.lib` (IJG libjpeg compress side + `CreateJPG`).
  `py -3.12 re/tools/fid_report.py --program <module>` -> `re/fid/<module>.fid.tsv`, read-only on a pool slot,
  applies nothing. `py -3.12 re/tools/fid_apply.py [--dry-run]` applies with Ghidra's ApplyFidEntriesCommand
  (USER/IMPORTED names untouched, `$L` labels reverted) -> `re/fid/<module>.fid_applied.tsv`. Applied to the master
  2026-10-03: exe 296, Terrain.dll 423, jgld.dll 270, sound.dll 287 renames (backup `ghidra/backup/`, not committed).
  Headless `-import` of a `.lib` does nothing ("Ignoring file '.'"); `BuildFidDb.java` imports the members itself.
- **.sve** `py -3.12 re/tools/sve.py info|diff|dump|check`: 100-byte header + raw dump of 86 globals (+ flag-gated
  tail) written by FUN_0040afa0 (`re/analysis/save/0040afa0_save_serializer.md`). 20 globals named (labels `g_*` in
  the master via `ghidra/scripts/LabelGlobals.java`; args `<va>#<name>`, the .bat splits on `=`); the rest: U-0004.
- **TTD** (WinDbg 1.2603 + TTD 1.11 installed): `py -3.12 re/tools/ttd_record.py` (one UAC prompt: ttd.exe
  -attach needs admin; the game runs unelevated) -> `log/ttd/<scenario>/*.run` (~1 GB). Query with
  `py -3.12 re/tools/ttd_query.py <run> --index` once, then `dx @$cursession.TTD.Memory(lo,hi,"r"|"w"|"e")`.
  The game runs far slower under TTD; ReadFile'd data is not a recorded write.

### Scenarios (`re/tools/scenario.py`)
`sandbox_basic` (menu -> Sandbox -> Monterey, 40 s), `course_season` (+ build tee/green with the Build
Course palette, open the hole with `H`, 240 s at x32 = ~6 game years), `championship` (installs the
fixture course, Play a Championship -> Easy -> course -> Gary Golf; the harness plays Gary's shots:
straight shot at the cup found by colour (red pennant + pale pole base) or the green centroid, zooming
out with X when the green is off-screen; 5/5 rounds finish in 45-107 s), `golfer_events` (organic golfer
info panel by clicking a golfer under its name label; then an INJECTED landmark notice 0x004722c0 called
on the game thread via re/frida/js/events.js), `golfer_stories` (4-hole course at x32 until the pair-story
advance FUN_00466370 returns 1; organic path toward the landmark, which needs both partners at chapter 4;
the 1-hole course can never advance a story because of the hole >= 2 gate, U-0003).
`stories_fixture` loads `tests/fixtures/stories/stories5.sve` (5 holes, not committed; README there) via
Continue Saved Game; the camera must be moved back over the course (`view_stories_course`) because story
advances only happen for golfers inside the screen rect. `stories9_fixture`: same with `stories9.sve` (Scotland, 9 holes;
do not zoom out: a 25 min zoomed-out run never passed stage 1). The camera is the tile pair at
0x4c2ba0/0x4c2ba4 (world->screen FUN_0042fb90); arrows move it 4 tiles, `events.js` `setCamera` 1 tile.
Golfer arrivals are capped by membership (type table 0x5849e0 +2): only member types arrive, so a save with
few members rarely produces story pairs, and a golfer whose mood drops below 0 quits and bans its type for good
(+0x29 = 0xff): `re/analysis/golfers/00406670_membership.md`, U-0003. Mood losses come from the rough, the
bad-lie complaint and the hole rating (FUN_0042dea0 flags 4 'too hard' / 8 'too easy'; short par 3s get 8 and every
birdie there costs mood). `mood5_fixture` (`mood5.sve`, 5 holes with fairways) keeps 8-16 golfers on the course for 15 min at x32 (bans still grow; best story stage 2). Chapters roll back because FUN_004669f0 recomputes the partner's chapter from its mood/counter and latest thought (`re/analysis/golfers/004669f0_story_reply.md`).
Census: `py -3.12 re/tools/coverage_census.py --scenario <name>`. All 6 patch-linked functions are now
reached (5 organically). Functions a scenario hooks with Frida are excluded from INT3 arming ("watched")
and recorded from the hook instead; injected calls use NativeFunction `exceptions: 'propagate'` so the
INT3 VEH sees them.
Input gotchas: posted mouse lParam is rescaled by Windows from the DPI-aware harness to the DPI-unaware
game (send physical coords: `Game._lp`); keys need WM_CHAR too; right click drops a build tool.

### Screenshots
`Game.shot()` BitBlts from the window DC (DWM redirection surface, unscaled 800x600, works for GDI and
OpenGL and while covered). Never grab screen pixels: another window on top produced false "hangs";
`PrintWindow` returned black frames 8-18% of the time (2026-10-02).

### Process hygiene
The game has **no single-instance guard**: a copy only failed because the `DWM8And16BitMitigation` compatibility
layer is registered for `original\golf_clean.exe`'s path; game.py passes it as `__COMPAT_LAYER` (2026-10-07).
Parallel A/B: `re/tools/instances.py create <name>` -> instances\<name>, then `SIMGOLF_ALLOW_MULTI=1
SIMGOLF_GAME_DIR=instances/<name>` for game.py / diff_hook, and `SIMGOLF_BUILD_DIR / SIMGOLF_DEPLOY_DIR /
SIMGOLF_RE_BATCHES` for build.bat. Keep copies at short paths (a Temp-path copy failed). Track the PID you spawn
and kill only that one. Without SIMGOLF_ALLOW_MULTI `Game.start()` refuses while another SimGolf window exists.
Never blanket-kill by name.
GDI font temp dirs (P2, 2026-10-31, log/roadmap/P2/notes.md): until P2 the port's GDI text provider (gditext.cpp) copied the two
game fonts into %TEMP%\sg_gditext_<pid>_<n>, accepted an existing directory and registered them session-wide
(AddFontResourceA). A killed test process left its .ttf locked by the session, and a later process that got the same pid failed
at record 0 with `missing text font file` / sg_run_boot -11 (I21 hit it three times in one day). Now every provider creates a
new directory and registers with FR_PRIVATE (negctl gditext 8 restores the old way; port/test/test_gditext_tempdir.py). A tree
built before P2 still leaks: release a dead pid's stale registration with RemoveFontResourceA on its .fot (deletes nothing).
Frida temp (2026-10-12): frida-core unpacks a 22-50 MB agent dir (%TEMP%rida-<hash>) per Python process and
removes it only on a clean exit, so runners killed on a timeout leak one each: 6,461 (326 GB) filled C: during round 29.
Game.start() now calls re/tools/game.py purge_frida_temp(): it deletes only dirs created before every running python /
node / frida process started (a live process's dir has no open file between attaches, so the first version, which trusted
a failed rename, deleted its own runner's dir and the next attach failed with `unable to find DLL`). Tools that attach
Frida without Game should call it too.
No audio output (LIVING-PC, 2026-10-13, found by c3ex): with waveOutGetNumDevs() = 0 initSound 0x483e90 gets 3 from the
music device open and freeSoundLib 0x484060 FreeLibrary's sound.dll ~1.3 s after the window, so every sound.dll key
ERRORs with `unable to find module`. re/tools/game.py then loads re/frida/js/keepsound.js, which pins sound.dll
(GetModuleHandleExA PIN) when the game's LoadLibraryA returns it; the game still runs its no-sound path (g_soundReady
0x83afc4 = 0), so sound code runs only when a fixture calls it. `SIMGOLF_KEEP_SOUND=0/1` overrides the detection.
Defender: `instances\` is excluded from scanning (2026-10-12; MARIANO-PC only, not set on LIVING-PC). Negctl builds that VirtualAlloc'd a flag page at the
fixed address 0x7A110000 were quarantined there as Trojan:Win32/Bearfoos.B!ml. All negctl sources now export the switch
instead (`<Batch>NegCtl[4]`, word 1 magic 0x5147454e; fixtures call `__freeze.negOn()` in re/frida/js/freeze.js after the
freeze); never reintroduce a fixed-address page. `original\` is still scanned: never deploy a negctl build there.

### LAN release of the port (mini-server)
`scripts/lan_release/` (2026-10-29, copied from SimCity's re/scripts/lan_release, itself TD5RE's): `publish_simgolf.bat`
(`publish.sh`) builds a clean `git archive HEAD` of port/ + re/model/include into port/build/lan_build, stages
port/build/lan_release with `stage_lan_release.py` (simgolf_port.exe, freetype.dll, the app-local VC143 runtime, game\ =
original\ minus the SafeDisc files, shim, dev logs, JPEG.lib and saved games; manifest.json of sha256 per file) and
`deploy_lan_release.sh` pushes only changed files over SSH to the td5re-files nginx: http://mariano-server.local:8088/simgolf/
(/home/mariano/td5re-release/simgolf). Player PCs: `update.bat` in an empty folder fetches `simgolf_update.ps1` and the build,
makes a "SimGolf (port)" shortcut; saves live in %LOCALAPPDATA%\SimGolfPort. The updater only updates the folder holding
it and refuses a git checkout (an early version fell back to the current directory and wrote into the repo).
Release rule (2026-10-29): the roadmap orchestrator publishes on its own after merges, when `scripts/lan_release/release_due.py`
says DUE (2 commits changing port/ or re/model/include since the commit named in the server manifest's label, or 1 once that
build is 24 h old); publish.sh appends each release to log/roadmap/lan_releases.tsv. Every deploy also deletes the server files the
new manifest no longer lists (refused above half of them unless PRUNE_FORCE=1), so the server holds one copy of the game.
Player PCs prune too (since 2026-10-29): update.bat deletes files under game\ the manifest no longer lists (`-NoPrune` keeps them;
above half of game\ it stops unless `-Prune` is given), never anything outside game\.
GitHub release (Mariano, 2026-11-03): every LAN publish also makes a GitHub release of the same commit on the PUBLIC repo nanofives/simgolfRE:
publish.sh pushes HEAD to origin/main and runs scripts/lan_release/github_release.py (tag port-<date>-<commit>, a zip of simgolf_port.exe,
freetype.dll, the VC runtime, a Play SimGolf.bat that takes SIMGOLF_INSTALL and a README with the FreeType credit; never game\, the
updater or manifest.json), logged in log/roadmap/github_releases.tsv. NO_GITHUB_RELEASE=1 skips it, GITHUB_RELEASE_NOTES=<file> sets the notes.

### SMSG-decomp snapshot (third_party/smsg)
`third_party/smsg/` (2026-10-29, copied on Mariano's instruction; PROVENANCE.md has the commits) holds github.com/SMSG-decomp: an
independent matching decompilation of the same v1.03 (claims all 5,982 functions at 100%, no rebuilt binary byte-exact), its
MSVC6-tooling (Rust compiler models, no permuter), Firaxis-decomp-notes and four PDBs on the original addresses (release/). Locally
only (gitignored): VC6 SP5 in tools/vc6sp5 (sound.dll's C++ objects were built by SP5's C2 12.00.8966, so our 8168 near-misses there
used the wrong back end), the SP2 debug CRT in tools/vc6sp2 (jgld / Terrain link it), the DirectX 8 SDK in tools/dxsdk8. Not copied:
MSVC6-decomp (decompiled Microsoft compiler; nothing here uses it). Their claims are hypotheses: a source counts when our match.py
gives 100% against our anchors (C4 match evidence), a name or field meaning when our disassembly supports it.
## Where things stand (2026-11-04)
- RE ladder: no C2 row is open (end states `dead` / `trivial` / held). Counts: `py -3.12 scripts/re_classify.py status
  --summary`. Upkeep: `re/roadmap/S11_c2_triage.tsv`.
- Port (`port/`, plan `re/PORT_PLAN.md`, conventions `port/CORE.md`, harness `port/test/README.md`): segments 205 / 205
  x64 + Win32, boot gate 67 PASS / 2 KNOWN / 2 NO_FRAMES (I25), census split 697 / 1. The app `simgolf_port.exe` defaults
  to profile `play`, `--wide`, layered HUD; tests run under `SG_APP_PROFILE=fixed` and the P5B / P6B / P8B / P9 `=off`
  switches set by `port/test/conftest.py`. Decisions: `re/roadmap/DECISIONS_PENDING.md`, `re/port/defect_decisions.tsv`.
- Gates: `py -3.12 scripts/gates.py --list` (one runner for all of them; `--tier fast|full`, one pytest file per process,
  full output to `log/gates/<run>/`, a PASS / FAIL table on stdout).

## Standing port rules
- Never run `pytest port/test` as one process (10-16 GB): one file per process (gates.py or `log/p38/suite_by_file.sh`).
- Port code cites functions by name: test_static refuses image-range literals, comments included (write flag bits as shifts).
- MSBuild does not rebuild registry.cpp when `src/sim/modules.inc` changes: touch it. Parallel children build in their own
  `port/build/<dir>` and load it with `SG_PORTSIM_DLL`.
- Golden digests are re-pinned only through `SG_EMU_ORACLE=update` after the oracle is GREEN.
- The corpus is never rewritten; renames go through `re/port/corpus_aliases.tsv` / `port/test/renames.py`.
- Every UI the port adds uses the game's fonts and art through the gfx backend.
- Heavy S3-style runs take `log/roadmap/S3/w8_lock.py`; long suites run detached (a background shell dies at 2 h).

## History (moved verbatim out of this file on 2026-11-04; read only the part a task needs)
- `re/history/MATCHING.md`: matching decompilation details (VC6 idioms per module, tools, flags).
- `re/history/C3_ROUNDS.md`: naming round 2, C3 rounds 1-35, C4 pilot, round 36 (C4), every finding and pitfall.
- `re/history/PORT_PHASES.md`: port pivot, batches b1-b6 / c3fy, phases 1 (model) .. 5 (gfx steps 1-2).
- `re/history/ROADMAP.md`: roadmap S0-S11, P3-P9 (gfx, terrain, audio, text, host app, video, net, defects, SMSG).
- Per-step notes: `log/c3/`, `log/p2/`, `log/p3/`, `log/roadmap/<section>/`; open questions `UNCERTAINTIES.md`.
New round / step write-ups go into the matching history file (or the step's notes), not here. Add to this file only a
standing rule or a fact every session needs.
### Patching
Patches are `patches/<name>.ini` with `expect` bytes taken from the anchored binary. Default
`enabled=0` unless it is a compat fix. Cite the analysis note in `desc`. The static test validates
every committed patch against the anchors.

## Delegation

Workspace policy (`..\CLAUDE.md`): read-only surveys go to the account2 worker via `delegate.ps1`.
Ghidra MCP, Frida, builds, game runs, tracker writes stay local.

## Happy info pane
Keep `.happy/project-info.json` current when a standing fact changes (status, counts, key dates).
