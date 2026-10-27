# S7.8 merge request (worker S7.8, 2026-10-25): U-1642 present point, U-1643 terrain log bound

Base: the S7.5 mirror (b3d61e41 + S3.2 + S7 waves 1-2), mirror port/build/S7_8_head (`~/bin/py -3.12 log/roadmap/S7/8_mirror.py
--build`). Notes: log/roadmap/S7/8_notes.md. Every file below is in the mirror; the patches are diffs against port/build/S7_5_head
(S7.5's accepted state), made by `~/bin/py -3.12 log/roadmap/S7/8_make_patches.py`.

## 1. Shared file (S7 lead): tick.cpp + tick_hook.h, `log/roadmap/S7/8_tick.patch`

Two hunks, nothing else changes for a trace run (`io.live` is null outside run_live):

- port/src/sim/tick_hook.h, `struct LiveIo`: a new virtual with a default body
  `virtual void present_point() {}` and its comment (the original's present: refreshFull -> Window::refreshRect -> drawTooltip
  -> Display::present).
- port/src/sim/tick.cpp, `TkBase::call`, case 1 (a drop), after the gfx_drop line:
  `if (io.live && std::strcmp(name, "Window::refreshFull") == 0) io.live->present_point();`

## 2. Step-owned files (copy from the mirror)

- port/include/portsim/sim/live.h: `LiveHost::present_point(Ctx &)` (default empty); LiveOptions `trace_file`, `spill_events`,
  `on_record` / `on_record_arg`, `keep_records`; LiveResult `trace_file_bytes`, `spilled_events`, `held_events` (defaults keep
  every earlier behaviour).
- port/src/sim/live.cpp: Core forwards `LiveIo::present_point` to the host; PrefixHost forwards it. Recorder's body encoding is
  an incremental `Encoder` (bytes() feeds every event at once, as before); a spilling recorder writes finished frames' events to
  `<trace_file>.body` and builds the file at the end; recorder control 9 (a spill drops its last event).
- port/app/host.h, host.cpp, main.cpp: presents at present_point (kind 'r'), counters `presents_by_kind`, `present_points`,
  `r_superseded` in session.json; TerrainLogBound (U-1643), RecordStreams (framehash.bin per record, trace.trace spilled by the
  core; `--in-memory` = S7.5's way) and the memory sampler (memory.tsv, session.json `memory`); `--negctl present-point-off`,
  `--negctl terrain-log-keep`, `--mem-every <n>`. No CMake change: K32GetProcessMemoryInfo is a kernel32 export (the build links without psapi.lib).
- port/test/test_app_present.py (new), port/test/test_app_ingame.py and test_app_live.py (present kinds: a frame's presents are
  'r' or 'w', the menu's hover presents 'r' or 'm'; facts files renamed to 8_*.json so S7.5 / S7.6 evidence is not overwritten).

## 3. Request to S3 (terrain_live.h / terrain_live.cpp; U-1643)

The app drops LiveTerrain's log lines at every frame end through `const_cast` on `LiveTerrain::log()` (host.cpp
TerrainLogBound::frame_end). Requested: an option that keeps no line, so the app stops reaching into the vector.

terrain_live.h, in `class LiveTerrain` public, after `const std::vector<std::string> &log() const { return log_; }`:

```cpp
  // roadmap S7.8 (U-1643): false = log no call line (a play session: nothing reads them); calls() counts every call either way
  void set_keep_log(bool keep) { keep_log_ = keep; }
  uint64_t calls() const { return calls_; }
```

and in the private members, after `std::vector<std::string> log_;`:

```cpp
  bool keep_log_ = true;
  uint64_t calls_ = 0;
```

terrain_live.cpp, `LiveTerrain::keep_snapshots`: replace both `log_.size()` with `calls_` (equal to log_.size() while every line
is kept). `LiveTerrain::call`: replace

```cpp
  std::string line = std::to_string(log_.size()) + " 0 0 " + name + " " + hex(self);
```

through `log_.push_back(std::move(line));` with the same lines inside `if (keep_log_) { ... }`, the first one reading
`std::string line = std::to_string(calls_) + " 0 0 " + name + " " + hex(self);`, followed (outside the if) by `++calls_;`
before `keep_snapshots();`. With keep_log_ true every line and every snapshot is byte-identical to today's.

Once it lands, host.cpp TerrainLogBound becomes: `live->set_keep_log(keep)` at attach, `calls()` = `live->calls()`, `length()`
= `live->log().size()`, and frame_end only records max_len (the const_cast goes).

## 4. Correction for re/analysis/sim/mainLoop_structure.md (row P6)

The row reads `the display present slot [[0x0083ad50]+0x9c] (0x00418cd1)`. Slot +0x9c of the Display vtable 0x1011d640 is
slot 39 = the thunk 0x10001875 -> Display::setScale 0x10067d50 (jgld_3.tsv: stores its three arguments into g_blitScaleX /
g_blitScaleY / g_blitScaleDen); P6 calls it with (1, 1, 1) (0x00418cbf..0x00418cd1, port tick_frame.inc P6_DisplaySlot9c already
says `the display's scale slot`). Display::present 0x10066470 is slot 41 (+0xa4). Replacement text:
`the display scale slot [[0x0083ad50]+0x9c] = Display::setScale(1, 1, 1) (0x00418cd1; the back buffer reaches the window in
Window::refreshFull -> refreshRect -> drawTooltip -> Display::present +0xa4 at 0x0047cfba, H1 0x0041e29b in an ordinary frame)`.

## 5. CLAUDE.md paragraph (after the S7 wave 2 paragraph)

(see 8_notes.md section 6)
