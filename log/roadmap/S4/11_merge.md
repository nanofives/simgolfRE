# S4.11 merge request (module audio_crt: sound.dll's static CRT and the four loaders' helpers)

Proven in the private build `port/build/S4_11_mirror` (log/roadmap/S4/11_mirror.py: HEAD + every S4 file of the tree + audio_crt.cpp +
the three shared edits below, applied by the script). Nothing below is applied to a shared file by this step.

## 1. port/src/sim/modules.inc

After the other S4 modules (`SG_MODULE(audio_core)` ... `SG_MODULE(audio_soundfile)`):

```
SG_MODULE(audio_crt)
```

Touch port/src/registry.cpp afterwards (MSBuild does not rebuild it when modules.inc changes).

## 2. port/src/sim/audio_core.cpp: the core asks audio_crt for the rvas it has no name for

The core finds a sound.dll rva in kSoundFunctions (hooks.csv names), then in `audio_crt_name`. Next to the declaration

```
const char *audio_crt_name(uint32_t rva);  // part 2
```

add

```
const char *audio_crt_function_name(uint32_t rva);  // S4.11: audio_crt.cpp
```

and in `audio_crt_name`, between `if (rva == snd_crt::ftol_) return "audio_core.crt.ftol";` and `return nullptr;`:

```
  if (const char *n = audio_crt_function_name(rva)) return n;  // S4.11: audio_crt.cpp
```

(S4.10's merge adds `audio_exe_sound_name` on the next line; 10_merge.md section 3.) Without these lines every call into the 28
functions is a development fallback again (the census in 11_notes.md section 3 counts 4,800 fallback calls).

## 3. port/test/sound_replay.py: step S4.11 in the harness tables

Add at the end of the module, after `def main` and before `if __name__ == "__main__":` (or anywhere after the tables and
`step_rows_all`), these two lines:

```
from port.test import sound_keys_crt as _s411  # noqa: E402  (S4.11: its rows, golden, custom keys, excluded keys, PORTED_STEPS)
_s411.install(sys.modules[__name__])
```

`install` adds step S4.11 to STEPS (golden_audio_crt.json), the custom keys to CUSTOM, four registry keys to EXCLUDED (U-1540) and
S4.11 to PORTED_STEPS (a fallback into one of its functions is RED for every other step), and gives a function without a row in
re/roadmap/S4_functions.tsv a synthetic one (section 4 makes them real). `port/test/sound_keys_crt.py` and
`port/test/sound_fixtures/s411_crt.js` are NEW files this step installed in the tree (the harness loads sound_fixtures/*.js on its
own); until the two lines are applied, `~/bin/py -3.12 log/roadmap/S4/11_replay.py <oracle|replay|negctl> --step S4.11` does the same.

## 4. re/roadmap/S4_functions.tsv

Nine rows exist with step `-`; change their first column to `S4.11` (nothing else): operator_delete 0x4249a, strstr 0x42690,
__ArrayUnwind 0x4288e, ehVectorConstructorIterator 0x42902, heap_alloc 0x42b0b, free 0x42c07, crtRealloc 0x45252, msize 0x45581,
_unlock 0x47771. Nineteen functions have no row; add (tab separated, columns of the file):

```
S4.11	crt	0x2e3c0	GlblHeader::init	C2	transcribed	0	emu-only	
S4.11	crt	0x05f40	Sound::getField1b8	C2	transcribed	0	emu-only	
S4.11	crt	0x2cf50	MixBuffer::deletingDtor	C2	transcribed	0	emu-only	
S4.11	crt	0x20480	Sound::queueMidiCommand3	C2	transcribed	0	emu-only	
S4.11	crt	0x244a0	ChannelDesc_dtor	C2	transcribed	0	emu-only	
S4.11	crt	0x29b80	VoiceBase_initListSlot	C2	transcribed	0	emu-only	
S4.11	crt	0x2c270	Ch5bb90_set3DBuffer	C2	transcribed	0	emu-only	
S4.11	crt	0x424b0	_strrchr	C2	transcribed	0	emu-only	
S4.11	crt	0x424d7	operator_new	C2	transcribed	0	emu-only	
S4.11	crt	0x42577	_sprintf	C2	transcribed	0	emu-only	
S4.11	crt	0x425c9	__onexit	C2	transcribed	0	emu-only	
S4.11	crt	0x42647	_atexit	C2	transcribed	0	emu-only	
S4.11	crt	0x4280e	ehVectorDestructorIterator	C2	transcribed	0	emu-only	
S4.11	crt	0x42acd	_malloc	C2	transcribed	0	emu-only	
S4.11	crt	0x42adf	__nh_malloc	C2	transcribed	0	emu-only	
S4.11	crt	0x45226	_lockexit	C2	transcribed	0	emu-only	
S4.11	crt	0x4522f	_unlockexit	C2	transcribed	0	emu-only	
S4.11	crt	0x45b80	__callnewh	C2	transcribed	0	emu-only	
S4.11	crt	0x47710	__lock	C2	transcribed	0	emu-only	
```

(The `level` / `status` columns stay as the lead's table defines them; the first nine keep theirs.)

## 5. The other steps' goldens and bundles (re-pin after sections 1-3)

With audio_crt registered, the other steps' calls into these 28 functions stop being fallbacks, so their pinned `fallbacks` / `pending`
fields (loadSoundFile: 282 calls and a pending list) and their recorded host calls change. The emulated original did not: re-record
with the final tree and keep the result.

```
export SG_EMU_ORACLE=1
for s in S4.2 S4.3 S4.4 S4.5 S4.6 S4.7 S4.9 S4.9b "S4.8 --family ds" "S4.8 --family dev"; do
  ~/bin/py -3.12 -m port.test.sound_replay oracle --step $s --update; done
~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.11 --update
```

`log/roadmap/S4/11_goldens/` holds the result of that run on the S4.11 mirror (golden_audio_{core,chan,ds,dev,mixer,codec,seq,sound,wave,
soundfile}.json): `~/bin/py -3.12 log/roadmap/S4/11_golden_compare.py` against the tree's files reports 1,026 keys, 0 with a different
digest list, tier, region list, x87 exit digest or vector count, and fallbacks 4,800 -> 0. They can be copied over the tree's goldens
instead of re-running (S4.10's golden_audio_exe.json is not in the set).

## 6. Tracker (not run by this step)

Names (re/names/sound_19.tsv, 19 rows; systems writeup re/analysis/systems/sound_19.md; names_validate, names_check_evidence and
names_sanitize pass):

```
~/bin/py -3.12 scripts/re_classify.py batch re/names/sound_19.tsv --to C1
~/bin/py -3.12 re/tools/c2_note.py --all --out log/c2_batch_s411.tsv
~/bin/py -3.12 scripts/re_classify.py batch log/c2_batch_s411.tsv --to C2
<ghidra>\support\analyzeHeadless.bat C:\Users\maria\Desktop\Proyectos\SimGolf SimGolf -process sound.dll -noanalysis -scriptPath ghidra\scripts -postScript ApplyNames.java re\names overwrite
~/bin/py -3.12 re/tools/c2_note.py --refresh --out log/c2_refresh.tsv && ~/bin/py -3.12 scripts/re_classify.py retag log/c2_refresh.tsv
```

The functions themselves are G1 emulator-judged (no hook source, no path-1 row for 24 of the 28): their S4_functions.tsv level stays C2
and the ladder's gate for C3 (a path-1 A/B) does not apply; strstr, __ArrayUnwind, ehVectorConstructorIterator and _unlock keep the
levels their path-1 rows give them and their registry keys are among the 61.

## 7. UNCERTAINTIES.md

Append the five rows of `log/roadmap/S4/11_uncertain.tsv` (U-1540 .. U-1544). U-1493 (S4.9b) is resolved: its four callees are
`GlblHeader::init` 0x2e3c0, `Sound::getField1b8` 0x5f40, `MixBuffer::deletingDtor` 0x2cf50 and `Sound::queueMidiCommand3` 0x20480
(re/names/sound_19.tsv), ported here. U-1411 (S4.2: no model of the static-CRT heap; S4.10's merge already resolves it by pointing at
audio_crt) now has its body, and S4.9b's pending list (strstr, _strrchr, the eh vector iterators, the four callees) is empty. Also: `test_audio_soundfile.py` and the goldens of section 5 no longer carry
`pending` entries; the sentence in its docstring ("pinned in the golden file ('pending'; S4.11)") can go.

## 8. CLAUDE.md (after the S4.10 paragraph)

```
Step S4.11, zero fallbacks in the sound.dll port (2026-10-25, notes log/roadmap/S4/11_notes.md, U-1540 .. U-1544): module audio_crt
(port/src/sim/audio_crt.cpp, 28 functions registered as snd.<name>): sound.dll's static CRT (the allocation chain operator_new /
_malloc / __nh_malloc / __callnewh / heap_alloc / free / operator_delete / msize / crtRealloc, __lock / _unlock and the exit-table
pair, _atexit / __onexit, _strrchr, strstr, _sprintf, the eh vector constructor / destructor iterators and __ArrayUnwind) and the
four unnamed callees of loadSoundFile (GlblHeader::init, Sound::getField1b8, MixBuffer::deletingDtor, Sound::queueMidiCommand3,
re/names/sound_19.tsv) plus ChannelDesc_dtor, VoiceBase_initListSlot (the element destructor / constructor the iterators are given) and
Ch5bb90_set3DBuffer (MultiSound::set3DBuffer's callee). The system-heap arm runs as port
code down to the HeapAlloc / HeapFree / HeapSize / HeapReAlloc imports; judged against the emulated original on 61 keys / 672 vectors in
layouts A, B, C (the keys use an import stand-in for the Heap* imports so both arms get the same block addresses, a new-handler stub
with a cell protocol for the failure paths, recording stubs for the small-block heap helpers, an EnterCriticalSection stand-in that
plays a second thread), 0 fallbacks, 28 / 28 mode-1 controls RED, every jcc of the originals reached on both sides except the
small-block arms of msize / crtRealloc (heap type 1 on every NT >= 5: U-1541), __lock's _amsg_exit arm and sprintf's __flsbuf arm.
Census: every S4 step's keys (1,026 over S4.2 .. S4.9b) re-run on the build with audio_crt: 0 fallback calls (were 4,800: S4.5 12,
S4.6 240, S4.7 315, S4.8 dev 36, S4.9 285, S4.9b 3,912), the emulated original's digests, tiers, regions and x87 exit digests
unchanged in all 1,026 (11_golden_compare.py), default replay x64 and Win32 PASS on the re-recorded bundles. The throw path of the
vector iterators (SEH, RaiseException) is not judged (U-1540: four registry keys excluded).
```
