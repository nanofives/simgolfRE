# b6 pending names for the parent (sound.dll speech codec), 2026-10-17

Batch b6 wrote `re/names/sound_18.tsv` and `re/names/sound_18_globals.tsv` (systems writeup `re/analysis/systems/sound_18.md`). The old rows were moved out of `sound_5/7/8/9.tsv` and the `_globals` files (a name or address may live in one names file only), text that quoted an old name in the remaining rows was refreshed in place, hooks.csv was synchronised with `re_classify.py retag re/names/sound_18.tsv` (23 name changes, levels unchanged, 23 CHANGELOG lines). ApplyNames, the Ghidra master and `re/functions_ghidra*.tsv` were not touched.

## To run

1. `ApplyNames.java re/names overwrite` headless on sound.dll (`-process sound.dll -noanalysis`): renames the 23 functions below and the four globals; USER names are replaced because of `overwrite`.
2. Two master labels have no replacement row and must be deleted by hand after step 1: `g_codecPermTable` at 0x1006447c (wrong address: the slot table is at 0x10064480, 0x1006447c is the last sign-mask entry) and `g_codecBitMasks` at 0x10064454 (the sign-mask table starts at 0x10064458; 0x10064454 is the last of the five flag values 2 4 8 16 32).
3. Re-export `re/functions_ghidra_sound.dll.tsv` (ListFunctions.java) and delete `log/xref_sound.dll.json`.

## Function renames (hooks.csv already follows)

| RVA | VA | old | new |
|---|---|---|---|
| 0x3a980 | 0x1003a980 | codecProcessBlock | codecDecodeFrame |
| 0x3aa90 | 0x1003aa90 | codecAllocContextA | codecAllocEncoderContext |
| 0x3aae0 | 0x1003aae0 | codecInitContextA | codecInitEncoderContext |
| 0x3aea0 | 0x1003aea0 | codecAllocContextB | codecAllocDecoderContext |
| 0x3aef0 | 0x1003aef0 | codecInitContextB | codecInitDecoderContext |
| 0x3b150 | 0x1003b150 | codecProcessResource | codecEncodeFrame |
| 0x3b230 | 0x1003b230 | codecSynthStage | codecSynthesizeFrame |
| 0x3b5a0 | 0x1003b5a0 | codecTransformStage | codecDecodeParams |
| 0x3c640 | 0x1003c640 | codecStageDispatch | codecAnalyzeFrame |
| 0x3d040 | 0x1003d040 | codecApplyFixedCoeffs | codecHighPassFrame |
| 0x3d0a0 | 0x1003d0a0 | applyIir2 | codecSynthPostFilter |
| 0x3e400 | 0x1003e400 | floatFloorToInt | floatRoundHalfAway |
| 0x3e520 | 0x1003e520 | decodeTableSymbol | codecCorrectNibble |
| 0x3e600 | 0x1003e600 | substituteIfBelowThreshold | codecRevertUnstableCoeffs |
| 0x3e6d0 | 0x1003e6d0 | gaussSolveClamped | codecLdlSolveClamped |
| 0x3e9f0 | 0x1003e9f0 | codecAutocorrelation | codecCovarianceMatrix |
| 0x3f310 | 0x1003f310 | codecGainSmoother | codecPitchTrackDp |
| 0x3f720 | 0x1003f720 | codecFeatureDecision | codecVoicingDecision |
| 0x40000 | 0x10040000 | codecPitchAnalysis | codecRefinePitchLag |
| 0x40330 | 0x10040330 | codecCorrelationLags | codecInverseFilter2 |
| 0x404d0 | 0x100404d0 | codecSymmetricFir | codecLowPassFir31 |
| 0x40da0 | 0x10040da0 | codecFirstOrderFilter | codecPreEmphasis |
| 0x40e30 | 0x10040e30 | codecBiquadFilter | codecHighPassFilter |

Rows kept with the same name and a corrected purpose (moved into sound_18.tsv): bitPermute 0x1003bf30, codecAmdfPitch 0x100414f0.

## Global names

| VA | old | new |
|---|---|---|
| 0x100b58a8 | g_codecMode | g_codecErrProtect |
| 0x100b58a4 | (none) | g_codecFrameLen |
| 0x10064480 | (g_codecPermTable was 0x1006447c) | g_codecFrameBitSlots |
| 0x10064458 | (g_codecBitMasks was 0x10064454) | g_codecCoeffSignMask |
| 0x10064a80 | g_codecSymbolTable | g_codecSymbolTable (text only) |

## Files outside re/names that still carry an old name (not edited, per the brief)

Registry keys, fixture keys and SG_HOOK name strings are free text; diff_hook and the gate look rows up by address, so the old spellings keep working. Rename them only together with their fixtures (`py -3.12 re/frida/diff_hook.py <name>` takes the registry key).

| old name | files |
|---|---|
| codecProcessBlock | re/frida/js/fixtures.d/c3bv.js, re/frida/js/fixtures.d/c3fn.js, re/frida/registry.d/c3bv.py, shim/src/re/c3bv.cpp, shim/src/re/c3fn.cpp, shim/src/re/c3fn_negctl.cpp |
| codecAllocContextA | re/frida/js/fixtures.d/c3dh.js, re/frida/js/fixtures.d/c3ew.js, shim/src/re/c3dh.cpp, shim/src/re/c3dh_cov.cpp, shim/src/re/c3dh_negctl.cpp, shim/src/re/c3ew.cpp, shim/src/re/c3ew_negctl.cpp |
| codecInitContextA | re/frida/js/fixtures.d/c3ca.js, re/frida/js/fixtures.d/c3dh.js, re/frida/registry.d/c3ca.py, shim/src/re/c3ca.cpp, shim/src/re/c3dh.cpp, shim/src/re/c3dh_cov.cpp |
| codecAllocContextB | shim/src/re/c3dh.cpp, shim/src/re/c3dh_cov.cpp, shim/src/re/c3dh_negctl.cpp |
| codecInitContextB | re/frida/js/fixtures.d/c3ca.js, re/frida/registry.d/c3ca.py, shim/src/re/c3ca.cpp, shim/src/re/c3dh.cpp, shim/src/re/c3dh_cov.cpp |
| codecProcessResource | re/frida/js/fixtures.d/c3bv.js, re/frida/js/fixtures.d/c3fn.js, re/frida/registry.d/c3bv.py, shim/src/re/c3bv.cpp, shim/src/re/c3fn.cpp, shim/src/re/c3fn_negctl.cpp |
| codecSynthStage | re/frida/js/fixtures.d/c3br.js, re/frida/js/fixtures.d/c3bv.js, re/frida/registry.d/c3bn.py, re/frida/registry.d/c3br.py, re/frida/registry.d/c3bv.py, shim/src/re/c3bn.cpp, shim/src/re/c3br.cpp, shim/src/re/c3bv.cpp, shim/src/re/c3ca.cpp |
| codecTransformStage | re/frida/js/fixtures.d/c3br.js, re/frida/js/fixtures.d/c3bv.js, re/frida/registry.d/c3br.py, re/frida/registry.d/c3bv.py, shim/src/re/c3br.cpp, shim/src/re/c3bv.cpp, shim/src/re/c3ca.cpp |
| codecStageDispatch | re/frida/js/fixtures.d/c3bv.js, re/frida/registry.d/c3bd.py, re/frida/registry.d/c3bi.py, re/frida/registry.d/c3bn.py, re/frida/registry.d/c3bv.py, shim/src/re/c3bd.cpp, shim/src/re/c3bi.cpp, shim/src/re/c3bn.cpp, shim/src/re/c3bv.cpp, shim/src/re/c3ca.cpp |
| codecApplyFixedCoeffs | re/frida/js/fixtures.d/c3au.js, re/frida/registry.d/c3au.py, re/frida/registry.d/c3bd.py, shim/src/re/c3au.cpp, shim/src/re/c3bd.cpp, shim/src/re/c3bv.cpp |
| applyIir2 | re/frida/js/fixtures.d/c3bd.js, re/frida/js/fixtures.d/c3br.js, re/frida/registry.d/c3bd.py, re/frida/registry.d/c3br.py, re/frida/registry.d/c3bv.py, shim/src/re/c3bd.cpp, shim/src/re/c3br.cpp |
| floatFloorToInt | re/frida/js/fixtures.d/c3az.js, re/frida/js/fixtures.d/c3ca.js, re/frida/registry.d/c3az.py, re/frida/registry.d/c3bi.py, re/frida/registry.d/c3bv.py, re/frida/registry.d/c3ca.py, shim/src/re/c3az.cpp, shim/src/re/c3bi.cpp, shim/src/re/c3bv.cpp, shim/src/re/c3ca.cpp |
| decodeTableSymbol | re/frida/js/fixtures.d/c3bd.js, re/frida/registry.d/c3bd.py, re/frida/registry.d/c3br.py, shim/src/re/c3bd.cpp, shim/src/re/c3br.cpp |
| substituteIfBelowThreshold | re/frida/js/fixtures.d/c3bd.js, re/frida/js/fixtures.d/c3bv.js, re/frida/registry.d/c3bd.py, shim/src/re/c3bd.cpp, shim/src/re/c3bv.cpp |
| gaussSolveClamped | re/frida/js/fixtures.d/c3bi.js, re/frida/js/fixtures.d/c3bv.js, re/frida/registry.d/c3bi.py, re/frida/registry.d/c3bv.py, shim/src/re/c3bi.cpp, shim/src/re/c3bv.cpp |
| codecAutocorrelation | re/frida/js/fixtures.d/c3bd.js, re/frida/js/fixtures.d/c3bv.js, re/frida/registry.d/c3bd.py, shim/src/re/c3bd.cpp, shim/src/re/c3bv.cpp |
| codecGainSmoother | re/C4_PLAN.md, re/frida/js/fixtures.d/c3bi.js, re/frida/js/fixtures.d/c3bv.js, re/frida/registry.d/c3bi.py, re/frida/registry.d/c3bv.py, shim/src/re/c3bi.cpp, shim/src/re/c3bv.cpp |
| codecFeatureDecision | re/frida/js/fixtures.d/c3bv.js, re/frida/registry.d/c3bv.py, shim/src/re/c3bv.cpp |
| codecPitchAnalysis | re/frida/js/fixtures.d/c3bn.js, re/frida/registry.d/c3bn.py, re/frida/registry.d/c3bv.py, shim/src/re/c3bn.cpp, shim/src/re/c3bv.cpp |
| codecCorrelationLags | re/frida/js/fixtures.d/c3bd.js, re/frida/registry.d/c3bd.py, shim/src/re/c3bd.cpp, shim/src/re/c3bv.cpp |
| codecSymmetricFir | re/frida/js/fixtures.d/c3bd.js, re/frida/registry.d/c3bd.py, shim/src/re/c3bd.cpp, shim/src/re/c3bv.cpp |
| codecFirstOrderFilter | re/frida/js/fixtures.d/c3az.js, re/frida/registry.d/c3az.py, shim/src/re/c3az.cpp, shim/src/re/c3bv.cpp |
| codecBiquadFilter | re/frida/js/fixtures.d/c3bd.js, re/frida/registry.d/c3bd.py, shim/src/re/c3bd.cpp |
| g_codecMode | re/frida/registry.d/c3ca.py |
| g_codecPermTable | - |
| g_codecBitMasks | - |

`CLAUDE.md` (history paragraphs of rounds 18-36) and `re/C4_PLAN.md` keep the old names as history. `hooks.csv` notes columns and `re/analysis/audio/<rva>_sound_<old name>.md` file names keep the old spelling (note paths are stored in the `note` column; regenerate with `c2_note.py` only if the file name should follow).

## MATCH lines

No `// MATCH:` line carries a `re/names` spelling: the lines in `re/match/m36c_D1_01.cpp` and `m36c_D2_01.cpp` use the f2c-style C symbols (`_analys_`, `_voicin_`, ...), the dbg_raw wrappers use `?FUN_<addr>@f_<addr>@@...`. 22 of the 38 f2c-spelled lines now differ from the behavioural name (list in `re/analysis/systems/sound_18.md`, section MATCH lines). Nothing needs editing for the names to stay consistent; if the parent wants the symbol spelling to follow, `re/match` must be edited and re-run with `match.py`.

