# 38i merge lines (semantic input actions, gate d)

Notes: log/p38/38i_notes.md. Nothing in tick.cpp changes (the batch-override hook of 38f is already merged), so there is no
overlap with stream 38j's tick.cpp patches. Rebased on 38j's merge: both patches below were regenerated against the
working tree of 2026-10-20 09:58 (38h and 38j merged) and pass `git apply --check`; the results in 38i_notes.md section 1
are on that tree.

## 1. port/src/sim/modules.inc
Append (the module port/src/sim/semantic.cpp + port/include/portsim/sim/semantic.h; until this line is in,
port/test/test_static.py::test_every_module_file_is_listed fails on the working tree, as for every new module):

```
SG_MODULE(semantic)
```

## 2. port/include/portsim/sim/commands.h (the four input-action kinds in CmdKind)
`git apply log/p38/38i/commands_h.patch`: adds, after the line
`  // 0x107.. reserved for the screens not ported yet (re/port/commands_split.tsv)`, the comment block and
`Key = 0x200`, `PointerDown = 0x201`, `Click = 0x202`, `TileClick = 0x203` with their argument layout. Additive; no existing
value changes.

## 3. port/src/sim/commands.cpp (apply_command forwards the new kinds)
`git apply log/p38/38i/commands_cpp.patch`: `#include "portsim/sim/semantic.h"` after `#include "portsim/sim/course.h"`, and
in apply_command the `default:` arm becomes

```
    default:
      if (is_semantic_input(cmd.kind)) return semantic_apply(ctx, cmd);  // phase 3.8 (38i): input actions 0x200..
      throw MissingResource{"command kind not implemented"};
```

No registration changes in commands.cpp (the module registers its own test entries `semantic.*`).

## 4. port/test/semantic.py (edited in place: additive)
New section "input actions (38i)" (xlate / gate, key_table, translate_batch, run_input, assign_targets, gate_segment) and the
`xlate` / `gate` subcommands; `--backend` now accepts win32. 38f's translate / run / diff are unchanged
(test_integrator.py's 15 tests pass on the 38i build).

## 5. UNCERTAINTIES.md
Rows U-1040 .. U-1042 from log/p38/38i_uncertain.tsv. Suggested update of U-0654 (status `partly resolved`, append to the
description): "38i (log/p38/38i_notes.md section 4): character keys arrive as rootWin_onChar(vk) then rootWin_onKey(ch, vk)
once or twice (the last character counts), non-character keys as rootWin_onKey(0, vk) alone (recorded pairs for Escape,
Tab, Enter, Backspace, the letters b d e f h l n p s t x z, '+', '-' and the non-character keys PageUp, PageDown, End, Home,
the arrows, F1..F10: portsim/sim/semantic.h); a WM_CHAR with no WM_KEYDOWN reaches rootWin_onKey with the code of the most
recent key-down of either kind (156 of 156 in the scenario traces, log/p38/38i/charonly_code.py). Not mapped: the other
keys' pairs (semantic.h takes their US-layout key), modifier combinations and the numpad codes 0x60..0x69."

## 6. CLAUDE.md (suggested paragraph, port section after "Phase 3.8 integrator")

Phase 3.8 stream 38i, gate (d) (2026-10-20, log/p38/38i_notes.md): INPUT ACTIONS, CmdKind 0x200.. (commands.h; module
`semantic`, portsim/sim/semantic.h): Key (75 named key actions, one per case of mainLoop H1's char switch and code switch,
plus PageDown, End, Backspace and a Char form), PointerDown, Click (with a ClickTarget: World + tile, Panel + kind + control,
Hud, Swing, Place, GolferAction, TerrainEdit, or Any for modal screens; checked against H2's routing on the state at the
dispatch point, CommandRefused when it does not hold) and TileClick. Each is LOWERED to the root-handler calls its input makes
(commands.cpp's handlers) at its dispatch point, so H1's key switch, H2's press code and routing and the modal screens react
exactly as to the raw input (U-1011 closed); what a lowering leaves out is write-only (the key release's g_char83821c,
g_key838208) or cannot differ (the handlers' deadline clock: g_audio5aa774 is 0 in every segment). `port/test/semantic.py
xlate | gate` translates every recorded dispatch (10,420 in the 165 COMMAND segments, 0 gaps) and runs a segment with every
batch replaced (integrator.setBatchOverrides): championship/00..02 PASS on x64 and Win32 with no RootHandler command, and all
165 semantic runs equal their raw runs digest for digest (165 PASS, the plan's 155 included, on the tree with 38j merged).
Findings: a WM_CHAR with no WM_KEYDOWN carries the code of the last key-down (U-0654); every cursor read equals the harness's
last cursor move (344,524 of 344,524), so an Aim / PointerMove kind is possible with a tick.cpp cursor hook (U-1040).
Private builds: log/p38/38i/mirror.py.
