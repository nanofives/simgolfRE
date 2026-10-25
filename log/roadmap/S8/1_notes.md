# S8.1 notes: movie and FLIC inventory (static, no game run)

Done 2026-10-24. Gate: re/port/movies.tsv + re/port/flics_census.tsv, proved complete by tests/test_movie_inventory.py.

## Result
- Two Bink movies, nothing else: intro (showTitleScreen 0x0043cd70, call 0x0043cd97, string 0x004c8a0c, immediate 0) and closing
  (mainLoop 0x0040f5c0, call 0x0040faa4, string 0x004c6bb4, immediate 1). Both reach BinkOpen only through playAudioFile 0x0043cce0
  -> BinkPlayer::play 0x00487090 (its only caller). Both files exist in original\Flics and extracted\cab (same sizes); headers
  BIKi 800x600 15 fps, 825 / 589 frames, one audio track.
- The brief's "nine imports" is sixteen: all listed in MOVIES.md section 2. 16 operand sites are inside the three BinkPlayer methods,
  73 are SafeDisc padding slots (excluded rows). jgld.dll / Terrain.dll / sound.dll import no binkw32 and name no movie.
- Skip paths per movie (file absent, BinkOpen NULL, no window) are in MOVIES.md section 1. The recorded runs already use the
  BinkOpen-NULL exit for the intro (shim `skip_intro`).
- FLICs: no cutscene. 14 directories, 1,893 files, all loaded by loadFlic 0x0043d740 (821 call sites in loadWorldAssets /
  loadInterfaceArt only), max frame 442x377. They are S3's sprite animations. 386 files are named by no call site.
- serviceMovie 0x0049c8e0 family = an unused RIFF/mmio movie player: table 0x00840698 / count 0x008406e4 have no writer, no mmioOpen
  import, count stays 0.
- Finding for S3: six loadInterfaceArt call sites name `Tree Cerc` / `Tree Drac` (space) while the install has `Tree_Cerc` / `Tree_Drac` (U-1701).

## Reproduce
```
~/bin/py -3.12 re/tools/movie_inventory.py          # rewrite both TSVs
~/bin/py -3.12 re/tools/movie_inventory.py --check  # OK / DIFFERENT
~/bin/py -3.12 -m pytest tests/test_movie_inventory.py -q   # 7 passed
```
Tool method: capstone linear sweep of each function range of re/functions_ghidra.tsv; every `E8` to playAudioFile / BinkPlayer::play /
loadFlic; every .text dword equal to a binkw32 slot; strings by regex over the four modules and install text files; a loadFlic name is a
`push imm` in the 14 instructions before the call whose string + `.flc` exists under original\Flics (two call arms can merge at one call).

## Controls
2 of 2 RED: a changed row call-site address (0x0040faa5) and a dropped slot row (0x00402863) each make `verify` return problems.
The test also reads the call and push bytes at the two movie sites straight from the exe, independent of the tool. 0 blind.

## Open
U-1700..U-1703 in log/roadmap/S8/1_uncertain.tsv. No merge note needed (no shared file touched). The unrelated working-tree changes
in git status are other workers'.
