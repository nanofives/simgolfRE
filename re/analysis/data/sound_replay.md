# sound.dll replay without the game (step S4.1)

The 980 sound.dll path-1 keys of `re/frida/registry.d` (module `sound.dll`, 33 batches, 15,498 vectors) were A/B'd inside the
live game: diff_hook.js called the original (MinHook trampoline) and the shim's reimplementation from a Frida script and
wrote one CSV per key to `log/diff/<rva>_sound_<key>.path1.csv`. Those CSVs keep only the formatted return value, an FNV-1a
hash of the state regions' hex, `state_changed` and `distinct_results`. Every state region is a fixture pointer
(`"$name"`), and the regions hold the run's own addresses (fixture blocks, list links, pointers into a sound.dll that was
relocated: its ImageBase 0x10000000 is taken by jgld.dll in the game). Neither those addresses nor the load base were
recorded.

This tool replays a key with no game: the fixture JS runs again, the ORIGINAL function runs in an emulator over the user's
own `original\sound.dll`, and the result is compared with the CSV on every field that does not depend on where things are.
The same machinery is the reference the S4.2 port runner compares against (section 6).

## 1. Parts

| file | what |
|---|---|
| `re/tools/fixvm/fixvm.js` | the fixture VM (Node): link protocol, per-key machine, PE import binding, diff_hook.js steps, shim test exports |
| `re/tools/fixvm/frida.js` | mock Frida API, evaluated inside each key's `vm` context (so ArrayBuffer and friends are the fixture's realm) |
| `re/tools/fixvm/mem.js` | the sparse 32-bit address space with dirty-page tracking |
| `re/tools/fixvm/syslibs.js` | fake system DLLs: export slots that trap, deterministic Win32 bodies, host calls |
| `port/test/sound_emu.py` | relocated image loader, `Machine` (Unicorn), `FixVM` (the link client), `KeyRun` (diff_hook.js for one key), CSV reader, Frida-exact formatting |
| `re/tools/sound_census.py` | the census of all 980 keys in three layouts -> `re/roadmap/S4_census.tsv`, details `log/roadmap/S4/1_census/<key>.json` |
| `re/tools/fixvm/controls.py` | negative controls (gate part c) -> `log/roadmap/S4/1_controls.tsv` |
| `re/tools/fixvm/x87_cw_measure.py` | control-word sensitivity of the c3fh caller-word keys (gate part d) -> `log/roadmap/S4/1_x87_cw.tsv` |
| `re/tools/fixvm/x87_probe_unicorn.py` | Unicorn's x87 against the Intel SDM on precision control and exception flags |
| `tests/test_fixvm.py` | 16 tests (static marker; Node and the DLL needed for the replays): about 35 s |

## 2. How to run

```
~/bin/py -3.12 re/tools/sound_census.py --jobs 8                 # all 980 keys, ~7 min on 12 cores; [N/M] key TIER lines
~/bin/py -3.12 re/tools/sound_census.py --keys Seq_setTempo_w27f  # some keys (updates their rows in the TSV)
~/bin/py -3.12 re/tools/sound_census.py --batch c3fh --no-init    # a batch, without the DllMain start-up
~/bin/py -3.12 re/tools/fixvm/controls.py                         # gate (c)
~/bin/py -3.12 re/tools/fixvm/x87_cw_measure.py                   # gate (d)
~/bin/py -3.12 -m pytest tests/test_fixvm.py
```

From Python:

```python
import sys; sys.path.insert(0, "port/test")
import sound_emu as E
with E.FixVM() as vm:
    res = E.KeyRun(vm, "Channel_setPanGains", "A").run()      # KeyResult: rows (orig, state_hex, state_orig, ...), fx, regions
    csv = E.read_csv(E.registry()[0]["Channel_setPanGains"], "Channel_setPanGains")
```

Node: `SG_NODE`, else `node` on PATH, else `C:\Program Files\nodejs\node.exe` (v24.20.0 used). Unicorn 2.1.4.

## 3. How one key is replayed

1. `KeyRun.setup`: the image is sound.dll's sections mapped at the layout's base with its 5,769 HIGHLOW relocations applied
   (of 5,816 relocation entries, 47 are type-0 block pads); the VM gets it once per base, binds every IAT slot to a fake
   DLL export and writes the real base into OptionalHeader.ImageBase as the loader does. A fresh vm context loads
   diff_hook.py's script parts in its order: keepalive, heapstandin, importstandin, filestandin, mmiostandin, the VM's own
   `__freeze`, recdisplay, uitree, modal, framemask, diff_fixtures, then every fragment of an enabled batch
   (`shim/re_batches.txt`), with the same `Memory.alloc(` -> `__keepAlloc(` rewrite. The real stand-in helpers run unchanged
   (heap arena, import stubs with their hand-encoded `fs:[0x24]` thread test, the mmio model). Then `DllMain(base, 1, 0)`
   runs in the emulator: the CRT start-up the live module had run before any key.
2. `fixture`: `DIFF_FIXTURES[name]()` runs; NativeFunction calls it makes into sound.dll (constructors, the x87 control-word
   stubs a fixture writes) are executed by the emulator in the middle of the fixture.
3. `regions`, `mkarm`: the state region bases are taken from the fixture object and the arm is built with the context's
   `NativeFunction` AFTER the fixture, exactly as diff_hook.js does (c3fo replaces `NativeFunction` to route both arms through
   its x87 probe).
4. Per vector: `resolve` (the `$name` arguments; c3fu's fixtures define getters that write memory at this moment, before
   the snapshot), snapshot of the regions, arm 1 (the original), hash, restore, arm 2, restore. Arm 2 runs the original again
   where the live run called the reimplementation, so writes outside the regions and JS-side counters advance as they did
   live; `second_arm` replaces it (section 6).

The emulated process: a 1 MB stack, a fake TEB behind fs (SEH end -1, thread id 0x1A2C at +0x24, LastError at +0x34), a flat
GDT, the x87 control word 0x027f (the rpc thread's). Registers are 0 at each call, the stack below esp is whatever earlier
calls left there (zero at the start) [U-1405].

## 4. What the mock models

| Frida API | VM |
|---|---|
| NativePointer | a uint32; arithmetic wraps at 32 bits; read/write through the address space; `toString()` `0x..`, `toString(16)` bare hex |
| Memory.alloc / __keepAlloc | bump allocator, zero-filled, sizes >= 4 KB page-aligned with a guard page, smaller ones 16-aligned [U-1408] |
| NativeCallback | a trap address; a call from emulated code comes back to the VM, arguments read by the declared abi; a JS exception is logged and the call returns 0 [U-1402] |
| NativeFunction | a fake DLL export runs its JS body, a NativeCallback its JS function, anything else is emulated; `double` returns are read with `fstp` at the current control word; +-Infinity reads as NaN |
| Interceptor.replace / replaceFast / revert | redirects at the target's first instruction; replaceFast's trampoline runs the original once past the redirect |
| Process / Module | sound.dll at the layout base (exports from its table), kernel32 user32 winmm ole32 dsound ntdll gdi32 msvcrt advapi32 kernelbase as fake modules; `getCurrentThreadId` = 0x1A2C, `enumerateThreads` = [game thread, rpc thread] |
| setTimeout / __freeze | timers queue and run only on request (live, they fired after the key); `__freeze.thread()` suspends nothing (one thread), `save / fill / setU32` write |
| writeDouble / writeFloat(+-Infinity) | quiet NaN, as Frida 17.22.1 did (c3fe notes) |

Fake DLL bodies (`syslibs.js`): the CRT's heap (HeapCreate / Alloc / ReAlloc / Size / Free, VirtualAlloc: a bump allocator in
an emulated-heap region), TLS (first index 27, measured, below), critical sections (Windows 8+ lock-bit protocol: a balanced
Enter / Leave restores the original bytes) [U-1404], version (6.2 build 9200, what Windows 8+ reports without a manifest),
command line, environment, startup info, module handles, GetProcAddress, Interlocked*, IsBad*Ptr (mapped pages),
CreateEvent / SetEvent / CloseHandle (fake handles), Sleep (nothing). Pure locale and code-page functions (GetACP, GetCPInfo,
MultiByteToWideChar, WideCharToMultiByte, GetStringTypeA/W, LCMapStringA/W, CompareStringA/W, GetLocaleInfoA/W,
IsProcessorFeaturePresent) are HOST CALLS: the emulator side calls the real function of the PC running the replay through
ctypes [U-1406]. Every other import has no body: a call raises `unmodelled import <DLL>!<name>`, ends the vector and is the
missing API the census names. The live run's import reached the real system there, so its result is not reproducible;
fixtures that needed it reproducible stood it in with `__importStandIn`, whose stubs call NativeCallbacks, not these bodies.

The shim's test exports a fixture calls (winmm.dll in the game is our shim): C3foX87Probe is hand-assembled from its naked
asm in shim/src/re/c3fo.cpp, its C3foProbeScrub call goes to a VM callback that does the memory edits in JS and the
80-to-64-bit conversion as native x87 code; C3foSehCall5 / C3fzaSehCall4 call their target with five / four stdcall
arguments (an exception under it is outside the model).

## 5. Position independence and the measured tiers

Each key runs in three layouts: A (sound.dll at 0x04C00000, fixture heap 0x30000000, emulated heap 0x60000000, no gaps),
B (0x05E30000, 0x48000000, 0x64000000, pseudo-random gaps between blocks, every page-sized block moved), C (0x07A50000,
0x52000000, 0x68000000, other gaps). A field whose text or bytes are equal in all three does not depend on a run address and
is compared with the CSV; otherwise it is counted as carrying one. Two layouts were not enough: Seq_allocTrackSlot's vector 1
returned an address-derived value that happened to be equal in A and B.

Census of 2026-10-24 (`re/roadmap/S4_census.tsv`; every one of the 980 keys has a row, all 980 CSVs are GREEN):

| tier | keys | meaning |
|---|---|---|
| EXACT | 181 | every field of every vector compared (no run address anywhere) and equal, distinct_results equal |
| EQUAL_PI | 785 | every position-independent field equal; some returns or state hashes carry run addresses |
| EQUAL_PI_DISTINCT_DIFF | 2 | cmdNode::scalarDeletingDtor, WaveInDeviceBase_scalarDtor: fields equal, distinct 10 against 5 [U-1403] |
| DIFF | 5 | `__ftol_snd_w27f/w37f/w67f/wa7f/wf7f`: Unicorn's x87 (below) |
| PARTIAL_ERR | 5 | KERNEL32!RaiseException (ehVectorConstructorIterator_snd_t0 / t3, sound___ArrayUnwind_t0 / t2: SEH dispatch not modelled), WINMM!timeGetTime (getPlaybackTime, 4 of 10 vectors) |
| FIXTURE_ERR | 2 | onWaveInCallbackMessage, postMidiNotification: the c3dh fixture waits for the running sound engine to set flag bit 5 |

Per vector (15,498): returns compared 13,257, equal 13,247, carrying a run address 2,196 (148 keys); state hashes (966
stateful keys) compared 4,194, equal 3,974 plus 210 equal with the x87 flag byte searched (below), carrying a run address
11,007 (760 keys; in 717 of them every row); state_changed compared 15,201, equal 15,201; vectors that stopped with an
error 21; distinct_results equal in 966 keys. The ten return and ten state differences are the `__ftol` rows of an empty
st(0).

Fixture styles (flags the VM records while the fixture runs): fixture memory 964 keys, sound.dll module accessed 734,
NativeCallback 685, frozen thread 511, writes to sound.dll's .data 509, Interceptor.replace 480, heap stand-in 320, import
stand-in 207, mmio stand-in 31, nothing allocated 5. Gate (b), one key per style, every vector equal (tests/test_fixvm.py
`STYLE_KEYS`): pure memory Channel_setPanGains (60 vectors, EXACT), sound.dll globals codecProcessBlock (14, EXACT),
NativeCallback vtables Midi_Device::setChannelVolume (14, EXACT; slot +0x40 a recording stdcall callback), heap stand-in
Channel_setFormat_oom (12, EXACT), import stand-in Midi_Device::flushMessages_h (10, EXACT), Interceptor.replace stubs
Channel_start_null (12, EXACT), frozen thread Channel_setRateGlide (160, EXACT), mmio stand-in parseRiffWave (12, EQUAL_PI:
all 31 mmio keys have every return comparable and equal; their state holds the stand-in's absolute buffer pointers and the
import log's caller addresses, so no mmio key is EXACT).

Facts the census established:
- The double printing of the CSVs depends on the Frida runtime of the run: the 2026-10-07 CSVs (nextRandomFloat,
  floatCopySign) carry the shortest form, the 2026-10-14 ones (c3fh `_st0` keys) the exact decimal expansion cut after 17
  significant digits (0.8 -> 0.80000000000000004, 1.1 -> 1.1000000000000000, 0.5 -> 0.5). `csv_double_style` picks by the
  CSV's meta timestamp [U-1401].
- The live process's TlsAlloc gave sound.dll's CRT index 27: the only value in 0..1099 for which _errno_a's import log hashes
  to its CSV; _errno_b and __doserrno_a / _b then match too.
- A NativeCallback whose JS throws: c3fq's converter stub calls `.toUInt32()` on a number; with the call returning 0
  Wave_Device::freeBufferList matches its CSV.
- Without DllMain only 5 keys change (the four errno keys, atexitDtor_100b49b8): `--no-init` census
  (`log/roadmap/S4/1_census_noinit.tsv`).

x87 (gate d). `x87_probe_unicorn.py`: Unicorn 2.1.4 applies the precision-control field (1/3 at 0x027f keeps 53 significant
bits, at 0x037f 64, at 0x007f 24) and never sets an x87 exception flag (PE after an inexact `fistp`, IE after an
out-of-range one, IE + SF for an empty register all read 0), and `fistp` of an empty register stores 0, not the integer
indefinite [U-1407]. On the CSVs: c3fo's `__ftol` keys compare the whole fnsave image; 12 exact inputs per key match, 42
inexact ones match once the status word's flag byte is set to 0x20 (PE) or 0x01 (IE), the flags the SDM gives for them
(`sound_census.py` searches that one byte), and the 2 empty-register inputs differ. `x87_cw_measure.py` over the 47 c3fh
caller-word keys: DsBuffer::setVolume and Seq_setTempo (both words) match their CSVs at the fixture's word and lose 1 and 9
matching fields when the word is swapped (SENSITIVE: the emulator's results at 0x027f and at 0x0f7f are the live ones); the
other 43 keep every matching field (their state hashes carry run addresses, so the CSV cannot see the word), although in 28
of them the emulator's own results change with the word on 1 to 11 vectors (`emu_rows_changed`): those are the rows a port
must reproduce at both words.

Controls (gate c, `controls.py`): flipping one fixture byte (bitUnpack's count, Channel_setFormat_oom's format block,
randRange's state), relocating for base + 0x10000 while mapping at base (getDsCoopLevel, Seq_setTempo_w27f,
Midi_resetFileState_p1; run without DllMain, whose start-up would stop first), and making every fixture NativeCallback return
its value + 1 (Channel_setFormat_oom, Seq_buildPlayOrder, Midi_Device::openOutput_null, crtValidatePointer_stand): 10 of 10
RED. Equivalent mutants: the stub control on 28 of the first 40 EXACT import / replace keys (their callbacks are recorders
whose return the function discards, or are not reached on the vectors), recorded with Midi_resetFileState_p1 and
ChanRecord_freeBuffers as BLIND.

## 6. Second executor (the S4.2 link)

Everything the VM does goes through one Python object, the executor (`Machine`). Another executor, the port, can sit next to
it in two ways:

- `KeyRun(..., second_arm=f)`: `f(run, args)` is called where diff_hook.js called the reimplementation, with the vector's
  raw arguments `[{t, v}]` (the VM's `curargs`), on the same memory (`run.m`), between the same snapshot and restore points.
  It returns `{"eax", "edx"}` (or `{"f"}` for a double); `Row.re`, `Row.state_re_hex` and `Row.match` then compare it with
  arm 1 like diff_hook's `same(row)`. tests/test_fixvm.py `test_second_arm_hook`: the original through the hook matches on
  every vector, a body off by one in eax reads RED on every vector. The port call needs the address space: `run.m.read`,
  the fixture's regions (`KeyResult.regions`) and every page the VM or the emulator wrote (`run.m.mapped`; read each page
  with `run.m.read`) give the image to copy into the port's context, and the port's writes come back with `run.m.write`.
  An indirect call the port makes to an address in the trap range (0x7A000000-0x7C000000: NativeCallbacks, fake DLL
  exports) is served by `run.vm.request({"op": "trap", "addr", "esp", "ecx", "edx"})` after the port has written its
  arguments where the abi puts them; the reply carries eax / edx / f and the bytes to pop.
- `KeyRun(..., machine_factory=g)`: `g(vm)` returns the executor. It needs `regions()`, `read`, `write`, `take_dirty()`,
  `apply_pages(pages)`, `set_hooks(hooks)`, `native_call(addr, abi, args, ret)`, `reg_reset()` and `mapped`. Native calls the
  fixture makes into sound.dll code still need the original, so a port executor keeps a `Machine` inside for those.

Link protocol (fixvm.js header): frames of `u32 length + JSON`, requests `{op, id}` both ways, replies `{op: "reply", id}`,
nested requests served while waiting; every message carries the sender's dirty pages `[[page index, base64]]`, VM messages
also the Interceptor redirects and trampolines when they change. Requests the VM serves: load, begin, fixture, regions,
mkarm, resolve, arm, curargs, trap, timers, cbpatch (controls), eval, state, quit; it makes call and hostcall.

## 7. Open items

- SEH dispatch (KERNEL32!RaiseException, RtlUnwind) is not modelled: 4 keys partly (the CRT EH helpers, not ported in S4).
- c3dh's onWaveInCallbackMessage / postMidiNotification wait for a running sound engine; getPlaybackTime reads the clock on 4
  vectors. Both are live-system dependencies, not fixture gaps.
- Rows whose hash carries a run address are counted, not compared. Pointer returns give the live address of some fixture
  blocks (CommandQueue::init returned 0x4002270 for `$q0`); placing blocks at recovered live addresses would make more
  hashes comparable. Not attempted.
- The two scalar-deleting destructors of U-1403.
- Unicorn's x87 exception flags and empty-register stores (U-1407): a port judged by this emulator on fnsave images must
  use the flag-byte search or another reference.

Uncertainties U-1400..U-1409: `log/roadmap/S4/1_uncertain.tsv`.
