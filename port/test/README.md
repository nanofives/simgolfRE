# port/test: the portsim test harness (phase 2.6, 2026-10-19)

Judges the C++ port (`portsim`) against the recorded behaviour of the original: the **vector corpus**
(port/corpus/vectors, re/analysis/data/vector_corpus.md, schema re/model/vector_schema.md) and the **segment corpus**
(port/corpus/segments, re/analysis/data/segment_corpus.md). No game, no Frida, no original binary is run at test time.
The corpora are local (gitignored); tests that need them skip when they are missing.

## One command

```
~/bin/py -3.12 -m pytest port/test            # builds portsim_c.dll when a source is newer, then runs everything
~/bin/py -3.12 -m pytest port/test -m static  # pure Python, no build
~/bin/py -3.12 -m port.test.report            # dashboard: every function and segment -> log/p2/p26_report_*.tsv
~/bin/py -3.12 port/test/build.py             # build only (x64 Release; --arch Win32 also builds, see "x87" below)
```

The build uses the Visual Studio 2022 Build Tools' CMake (`port/CMakeLists.txt`, targets `portsim` static and
`portsim_c` shared). `SG_PORTSIM_DLL=<path>` makes the harness load another DLL.

## Pieces

| file | what |
|---|---|
| `port/include/portsim_c.h`, `port/src/portsim_c.cpp` | the C ABI: state, 32-bit address space, `sg_call` by name, registry info, negative-control switches, object table and sgh64 hashes, `sg_run_frames` (final signature, `SG_E_NOT_IMPLEMENTED` until 3.7 / 3.8) |
| `port/include/portsim/registry.h`, `port/src/registry.cpp` | what a port module uses: `Ctx` (GameState + `Memory`), `Ret`, `FuncDef`, `ModuleDef`, `SG_NEGCTL`, `ftol_low32` |
| `port/include/portsim/state_hash.h`, `port/src/state_hash.cpp` | the object table from gamestate_meta.inc and sgh64 (= re/tools/state_image.py) |
| `port/src/sim/modules.inc` | the list of port modules (one `SG_MODULE(name)` line each) |
| `port/src/sim/util.cpp` | first client: Random::next, Random::range, clamp, distance |
| `lib.py` | ctypes binding (`lib.load()`, `State`) |
| `vectors.py` | `VectorRunner` |
| `segments.py` | `SegmentRunner`, `first_divergent_record`, `first_byte_divergence` |
| `report.py` | the dashboard |
| `test_static.py`, `test_vectors.py`, `test_segments.py` | the tests (markers `static`, `port`) |

## The address space

The original is 32-bit and its state holds 32-bit pointers (the model stores them as `uint32_t`). A context owns a
32-bit virtual address space: the GameState at a base the caller sets, plus the caller's mappings. A pointer argument,
a pointer read from state and a pointer return value are virtual addresses; port code resolves them with
`ctx.mem.ref<T>(va)`, `load<T>`, `store<T>` or `at(va, len, write)`, which throw `portsim::Fault` (reported as
`SG_E_FAULT`) when no mapping covers the access or a read-only mapping is written. The harness maps the GameState at the
original data section's base and the fixture blocks at fresh bases (page offsets differ from the capture's, so a missed
relocation cannot pass by accident), and maps the user's own golf_clean.exe sections (read-only) when `original/` holds it.
Port code never contains an address: the bases are the caller's (`test_static.py` refuses exe / DLL-range literals).

## Adding a function (phase 3 children)

1. Write the function in your module file `port/src/sim/<module>.cpp` (declarations in `port/include/portsim/sim/<module>.h`)
   from the specification: the analysis note, the system writeup, the vector corpus. Hook sources in shim/src/re and
   re/match are reading references only.
2. Add an adapter with the original's interface and list it in the module's `FuncDef` table:
   ```cpp
   void a_myfn(Ctx &ctx, const uint32_t *args, Ret &ret) {           // args = the original's 32-bit slots
     ret.set_i32(myfn(ctx.gs(), static_cast<int32_t>(args[0])));     // thiscall: args[0] is `this` (a virtual address)
   }
   const FuncDef kFuncs[] = {{"myFn", "default", "int", 1, 0, a_myfn, &g_negctl_myfn}, ...};
   extern const ModuleDef kModule_<module> = {"<module>", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};
   ```
   `name` is the hooks.csv name (the .vec file is `<name with :: -> _>.vec`); `abi`, `ret` and `arity` must equal every
   corpus key of the function (`test_registry_matches_corpus` checks); `fpcw` is 0x027f when the original's result
   depends on x87 code (the corpus `x87` flag), else 0. A new module file also needs its line in `modules.inc`
   (`test_every_module_file_is_listed`); the CMake glob picks the file up.
3. Run `~/bin/py -3.12 -m pytest port/test -k <name>`. `test_function_green_on_vectors[<name>]` prints the summary.
4. Read the summary, not only the verdict:
   - `skipped`: vectors not run and why (`status ...`, `reloc_check false`, `raw location`, `fpcw ... unsupported`).
   - `distinct`: distinct expected (ret, out) pairs among the vectors run.
   - `WARNING only N vectors` (< 10) or `zero spread`: the vectors prove little; say so in your report.
   - `WARNING branch coverage one_sided / partial_branch / thin / none` (re/port/vectors_coverage.tsv): the vectors miss
     branches; the function must also be covered by segments (re/PHASE23_PLAN.md section 8.6): report which.
   - a failure names the key, the vector index and the first difference (`ret`, `out <symbol>+off`, `extra write ...`:
     a byte changed outside the recorded out regions).
5. Add a negative control: a deliberately wrong body selected by the function's switch (`int g_negctl_<x> = 0;` in the
   module, read through `SG_NEGCTL(g_negctl_<x>)`, set from Python with `lib.negctl(name, mode)`, never a fixed-address
   page) and a test that every mode reads RED on the vectors (see `test_negctl_random_range_reads_red`). A wrong detail
   that reads GREEN means the vectors cannot see it: record that.
6. Defects of the original are reproduced bit for bit (decision 4), behind named compatibility switches when a fix is
   ever wanted (re/analysis/data/model_defects.md).

## What a vector run does

re/analysis/data/vector_corpus.md section 8: per vector a zero GameState, the key's `seed` then the vector's `in`, the
blocks copied and mapped, `relocs` written as virtual addresses, the x87 control word set (`sg_set_fpcw`), the call,
then `ret` (pointers and `ret_loc` mapped back to locations, NaN / inf through `float_repr`), every `out` region (with
`out_ptrs` as addresses) and no other changed byte in the GameState or the blocks. A write of the value already there is
invisible to this diff (the emulation's exact written-set check is stricter, U-0722).

## Segments

`SegmentRunner.run(load_segment(dir))`: start.data.bin into the GameState, frames.json symbols = the library's 203 sim
objects in order, no noise dword in a sim object, the C++ sgh64 of the 203 sim objects = frames.bin record 0 (the load
check), then `sg_run_frames`. Until 3.7 / 3.8 provide the frame step the result is `LOADED_ONLY`; afterwards every record
(T and 203 hashes) and the end checkpoint's sim hashes are compared and a failure names the first divergent record and
objects. `first_byte_divergence(lib, image_a, image_b, classes)` gives the first differing byte (symbol, offset) of two
state images, pointer words ignored.

## x87

On x64 the port's doubles are SSE2 (53-bit significand, round to nearest), which equals the original's x87 word 0x027f
for single correctly rounded operations (+, -, *, /, sqrt) and integer conversions; `sg_set_fpcw` accepts 0x027f there
and refuses any other precision or rounding (`SG_E_UNSUPPORTED`, the vectors are then skipped with that reason). Code
whose result depends on 80-bit intermediates or another control word (sound.dll's 0x0f7f mixers, c3fh) needs explicit
emulation in the port; the Win32 build sets the word with `fldcw` but the 64-bit Python harness cannot load it (U-0721).
