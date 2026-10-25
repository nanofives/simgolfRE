# S1.5 merge notes (apply by the integrator; the shared files below were NOT edited in the tree)

Built and gated in the private mirror `log/roadmap/S1/5/mirror.py` (HEAD for every tracked port/ file a sibling changed, the
working tree for this step's own files, then `log/roadmap/S1/5/patches.py` applied): port/build/S1_5 (x64), port/build/S1_5_w32
(Win32). The base build for the comparison is the same mirror with `S15_BASE=1` (HEAD alone): port/build/S1_5_base.

## 1. port/src/sim/tick.cpp, parse_trace (REQUIRED: without it a recording's bio keys never reach the EditBox)

The five replacements are in `log/roadmap/S1/5/patches.py` (`PATCHES["src/sim/tick.cpp"]`, each `old` occurs once at HEAD
4bfefc2a; `mirror.py` asserts it). In words:

(a) after `bool after_mousemove = false;` (before `while (p + 4 <= n) {`): the comment block `KEYS THE WINDOW TREE'S FOCUS
CHILD TOOK ...`, a `PendKey pend_key` (msg, wParam), a `std::vector<Command> focus_held`, and two lambdas: `focus_resolve()`
turns a pending key MSG record into a RootHandler command 12 (WM_KEYDOWN, a0 = wParam) or 13 (WM_CHAR, a0 = wParam) held for
the next batch (negative control `tick.frame` mode 8 drops it); `focus_flush()` appends the held commands to the open batch.
(b) CLOCKRUN outside handler code: `focus_resolve(); focus_flush();` before `ctx_ra = ra; open_batch = false;`.
(c) a new branch `else if (type == 0x05 && len >= 16)` (MSG) before the CURSOR branch: `focus_resolve(); focus_flush();` then
a WM_KEYDOWN (0x100) / WM_CHAR (0x102) record becomes the pending key.
(d) HANDLER: `pend_key.on = false;` first (the key reached a root handler), and `focus_flush();` right after `cur =
&f.batches.back();`, before the handler's command is pushed.
(e) FRAME: `focus_resolve(); focus_flush(); focus_held.clear();` first.

Measured effect on everything before this step: none. In the 64 corpus traces all 11,312 WM_KEYDOWN / WM_CHAR MSG records
have a root HANDLER after them (`log/roadmap/S1/5/unmatched_keys2.py`: 5,265 key-downs -> onChar, 159 key-downs with no
character -> onKey(0, vk), the WM_CHARs -> onKey), so no command 12 / 13 is made and no batch changes. Gates with the patch:
segments 205 / 205 PASS x64 and Win32 with every digest equal to the HEAD build's (5_segments_S1_5.tsv,
5_segments_S1_5_w32.tsv vs 5_segments_S1_5_base.tsv); boot gate 62 PASS / 2 KNOWN / 1 NO_FRAMES, identical row by row to the
HEAD build except the new cov_r_bio (HEAD build FAIL at record 579, MissingResource on the bio; this build PASS 687 / 687)
(5_boot_gate.tsv vs 5_boot_gate_base.tsv).


### Exact text (old -> new, in order; each `old` occurs once in tick.cpp at HEAD 4bfefc2a)

#### 1. replace

```cpp
  bool after_mousemove = false;
  while (p + 4 <= n) {
```

with

```cpp
  bool after_mousemove = false;
  // KEYS THE WINDOW TREE'S FOCUS CHILD TOOK (roadmap S1.5, U-1032; log/roadmap/S1/5_notes.md). A WM_KEYDOWN reaches the
  // root's onChar (HANDLER 8; a key with no character, MapVirtualKeyA 0, the root's onKey with a0 = 0: HANDLER 7) and a
  // WM_CHAR the root's onKey (HANDLER 7) only when no focused child window returns non-zero for it first (uiKeyEvent ->
  // onCommand -> dispatchAccel, uiKeyEvent148 -> Window::onKeyChar -> Window::key, which ask the root's focus ring before
  // the root's own slot). A key MSG record that no HANDLER record follows before the next MSG record, clock read or frame
  // head was taken by such a child (the character editor's bio EditBox: EditBox::onKey, EditBox::insertChar): it becomes a
  // RootHandler command 12 (WM_KEYDOWN, a0 = the virtual key) or 13 (WM_CHAR, a0 = the character), which apply_command
  // treats as nothing and the bio entry routes to its EditBox (chareditor.cpp). Such commands only join a batch some
  // HANDLER record opens (the open one, or the next one of the same frame), so no batch is added. In the 64 corpus traces
  // every one of the 11,312 WM_KEYDOWN / WM_CHAR MSG records is followed by a root handler, so none of them gets one.
  struct PendKey {
    bool on = false;
    uint32_t msg = 0, wp = 0;
  } pend_key;
  std::vector<Command> focus_held;
  auto focus_resolve = [&]() {  // the pending key MSG record got no matching root handler
    if (!pend_key.on) return;
    Command c;
    c.kind = CmdKind::RootHandler;
    c.handler = pend_key.msg == 0x100 ? 12u : 13u;
    c.a0 = static_cast<int32_t>(pend_key.wp);
    if (SG_NEGCTL(tk::g_negctl_frame) != 8) focus_held.push_back(c);  // negative control 8: the commands dropped
    pend_key.on = false;
  };
  auto focus_flush = [&]() {  // into the open batch, after the handlers it holds so far
    if (open_batch && cur && !focus_held.empty()) {
      cur->cmds.insert(cur->cmds.end(), focus_held.begin(), focus_held.end());
      focus_held.clear();
    }
  };
  while (p + 4 <= n) {
```

#### 2. replace

```cpp
      } else {
        ctx_ra = ra;
        open_batch = false;
      }
    } else if (type == 0x08 && sub == 1) {
```

with

```cpp
      } else {
        focus_resolve();
        focus_flush();
        ctx_ra = ra;
        open_batch = false;
      }
    } else if (type == 0x05 && len >= 16) {  // MSG: pos, msg, wParam, lParam (the keys the focus child may take, above)
      focus_resolve();
      focus_flush();
      const uint32_t msg = rd32(q + 4);
      if (msg == 0x100 || msg == 0x102) pend_key = {true, msg, rd32(q + 8)};
    } else if (type == 0x08 && sub == 1) {
```

#### 3. replace

```cpp
    } else if (type == 0x09) {  // HANDLER (id = sub): pos, a0, a1
      if (!frames.empty()) {
        TraceFrame &f = frames.back();
```

with

```cpp
    } else if (type == 0x09) {  // HANDLER (id = sub): pos, a0, a1
      pend_key.on = false;  // the key reached a root handler
      if (!frames.empty()) {
        TraceFrame &f = frames.back();
```

#### 4. replace

```cpp
        cur = &f.batches.back();
        Command c;
        c.kind = CmdKind::RootHandler;
        c.handler = sub;
```

with

```cpp
        cur = &f.batches.back();
        focus_flush();
        Command c;
        c.kind = CmdKind::RootHandler;
        c.handler = sub;
```

#### 5. replace

```cpp
    } else if (type == 0x02) {  // FRAME: frame, pos, t, snapshot
      TraceFrame f;
```

with

```cpp
    } else if (type == 0x02) {  // FRAME: frame, pos, t, snapshot
      focus_resolve();
      focus_flush();
      focus_held.clear();  // a taken key with no batch of its frame to join (none in the corpus)
      TraceFrame f;
```

## 2. port/include/portsim/sim/commands.h (comment only)

In `struct Command`, the RootHandler handler list: append
`12 a WM_KEYDOWN and 13 a WM_CHAR that a focused child window took before any root handler (tick.cpp parse_trace; a0 =
wParam; apply_command does nothing with them, the bio entry in chareditor.cpp gives them to its EditBox)`.

## 3. port/include/portsim/sim/chareditor.h (comment only)

Replace the sentence `The bio entry (an EditBox in the window tree) is not ported: a press on it throws MissingResource before
any of its writes.` by `The bio entry (control 20, non-members) edits the record's bio through the EditBox family registered by
this module (EditBox::create / setText / resizeBuffer / insertChar / onKey / resetFields / releaseState; roadmap S1.5); keys the
window tree gives the EditBox arrive as RootHandler commands 12 / 13 (commands.h). Up / Down need the font's line wrap and
throw MissingResource (U-1190).`

## 4. UNCERTAINTIES.md

- Row U-1032: status `resolved (S1.5, 2026-10-24)`; append to the description: "Resolved: chareditor.cpp ports the bio over
  the EditBox family (create, setText, resizeBuffer, insertChar, onKey without Up / Down, resetFields, releaseState); keys the
  focus child takes come from tick.cpp parse_trace as RootHandler commands 12 / 13. Judged by port/test/test_chareditor_bio.py
  (emulator oracle on the original with the key routing for real: 7 x 300 EditBox cases and 120 editor cases with 212 bio
  entries GREEN; test_chareditor.py's oracle runs the bio too, 300 / 300) and by the pinned scenario cov_r_bio (boot gate
  PASS 687 / 687, 5 controls RED). Residual: U-1190..U-1195."
- Append the six rows of `log/roadmap/S1/5_uncertain.tsv` (U-1190..U-1195).

## 5. CLAUDE.md (optional, one paragraph after the S1 lines)

"S1.5 (2026-10-24, U-1032): the character editor's bio entry is ported (chareditor.cpp: the EditBox family, the bio loop). The
original routes keys to a focused EditBox through the window tree (Window::onKeyChar -> Window::key -> EditBox::insertChar,
uiKeyEvent -> onCommand -> dispatchAccel -> EditBox::onKey), so a key it takes reaches no root handler and leaves a MSG record
with no HANDLER record; tick.cpp parse_trace makes those RootHandler commands 12 / 13 (none exist in the 64 older traces).
The bio loop makes no clock call, so scenario_det.js cannot poll there: cov_r_bio's agent carries an extra part that posts its
keys through the agent's doAction at the loop's leaveModal483d80 call (re/tools/scenario_s1_5.py), so the trace has a DELIVER
before each message and validates. Oracle: port/test/test_chareditor_bio.py (SG_EMU_ORACLE=1)."

## 6. Not changed, worth knowing

- port/test/trace_commands.py (the Python mirror of the batching) does not make commands 12 / 13; port/test/semantic.py's
  lowering has no kind for them (U-1193).
- re/analysis/data/scenario_refs.json gained only the cov_r_bio entry (regen's write_manifest; diff checked against the copy
  taken before: 0 other entries changed, `updated` stamp only).
- port/test/golden_chareditor.json: showCharacterEditor cases 43, 61, 90 re-pinned (the three golden cases whose script presses
  the bio of a non-member) and the keys `bio`, `EditBox::*` added, all through SG_EMU_ORACLE=update after the oracle was GREEN;
  every other key unchanged.
