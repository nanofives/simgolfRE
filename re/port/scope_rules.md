# Port scope rules (phase 2.0, 2026-10-18)

Generated rows: `re/port/scope.tsv` by `re/tools/port_scope.py` (`classify()` in that file is the normative text of these rules;
`tests/test_port_scope.py` checks them on a synthetic graph). Rules fire in order, first match wins. Inputs per function:
Ghidra list entry/size (re/functions_ghidra.tsv, 2,934), subsystem tag (hooks.csv, re/names, neighbour propagation as in
model_inventory.py: `tag_src` own / callers / callees / neighbours), mainLoop phase reach and boot reach (model_inventory.reach_map: a
call site inside mainLoop's phase ranges seeds a walk down the call graph; ENTRY 0x004a682f seeds the boot set), and the data
objects the function references (model_scan references mapped to the 989 objects of port/include/gamestate_meta.inc, class sim /
hidden / ui / pointer / render / audio / engine / const).

Definitions. SIM_PHASES = PRE, P1..P7, H1, H2 (the frame body). SESSION_PHASES = SETUP, SESSION, QUIT, MENU. A *sim object* has class
`sim`; `hidden` objects (g_rng, g_lastTipDate ...) count as simulation state for the rules but are listed in their own column.
A *heap object* is a ui / pointer / render / audio class object, except the stream-F constant tables (`f_ptrtab_*`, `f_strpool_*`,
`f_tab_*`: reading a string-pointer table is not a heap dereference). *Drawing name*: `^(draw|show|render|paint|panel|window|menu|blit|
Surface|Image|Widget|View|Button|EditBox|ListBox|TextView|scroll)`. *Drawing callee*: a direct callee tagged render or video.

| id | module | condition |
|---|---|---|
| R0-MAINLOOP | sim-core | entry 0x0040f5c0 (one 73,813-byte body; kept whole, rebuilt by phase 3.7/3.8, not a leaf-first item) |
| R1-RANGE | crt-dropped | entry >= 0x004a4ed0 (CRT, STL, SEH) and not a libjpeg name |
| R2-LIB | formats-loaders | entry >= 0x004a4ed0, < 0x004b7000 and a libjpeg-shaped name (IJG 6a compress side) |
| R1-TAG | crt-dropped | tag `crt` |
| R3-NET | net | tag `net`, or entry in 0x00497b40..0x0049bec0 (Jackal network layer) |
| R4-AUDIO-TAG | backend-audio | tag `audio` |
| R5-BOOT-UNREACHED | boot-only | no mainLoop phase reaches it and the boot walk from ENTRY does |
| R5-BOOT-TAG | boot-only | tag `boot` and no SIM phase reaches it |
| R6-SIM | sim-core | touches >= 1 sim or hidden object, tag in {golfer, sim, economy, ai, course, terrain, util, none}, reached from a SIM phase, name is not a drawing name |
| R6-SIM-SESSION | sim-core | as R6-SIM but reached only from SETUP / SESSION / QUIT / MENU (new-game setup: initRecords, loaders of session state) |
| R6-SIM-INDIRECT | sim-core | as R6-SIM, no phase reaches it (vtable / stored-pointer callee) |
| R6-SIM-RETAG | sim-core | touches sim state, reached from a SIM phase, tag in the ui / input / frontend / render / video family (usually inherited from neighbours or callers), no heap object, not a drawing name, no drawing callee |
| R7-GFX-TAG | backend-gfx | tag `render` or `video` |
| R8-UI-NAME | ui | sim-family tag but a drawing name (showXScreen, drawXPanel): a straddler when it touches sim state |
| R7-TERRAIN | backend-gfx | tag `terrain` and (a heap object or a drawing name); terrain helpers without either wait for R10 |
| R8-UI-TAG | ui | tag `ui`, `input` or `frontend` |
| R9-SAVE | formats-loaders | tag `save` (the .sve serializer touches sim objects: model/sve_fields.tsv is its specification) |
| R9-FMT-NAME | formats-loaders | name `load* / decode* / parse* / Image::load|save / Flic* / Bink* / save* / writePcx|Bmp*` and no sim object |
| R4-AUDIO-OBJ / R7-GFX-OBJ / R8-UI-OBJ | backend-audio / backend-gfx / ui | no sim object, touches audio / render / other heap objects |
| R11-GLUE-NAME | engine-glue | thunk, adjustor, vtordisp, dynInit, atexit, scalar / deleting destructor names |
| R10-LEAF | sim-leaf-util | fixed point: a direct callee of a sim-core / sim-leaf-util function with no heap object, tag not in {input, render, video, audio, net, crt, save, boot}, not mainLoop, entry < 0x004a4ed0, and (for ui-family tags) neither a drawing name nor a drawing callee |
| R7-TERRAIN-LATE | backend-gfx | terrain-tagged, left over after R10 |
| R11-ENGINE / R11-NOTOUCH | engine-glue | left over, touches only engine objects / touches no data object |
| R12-UNKNOWN | unknown | everything else |

A function is a **straddler** when it touches a sim or hidden object and either a heap object or lives in the ui / backend-gfx module
(`re/port/scope_straddlers.tsv`: sim, hidden and heap object lists). For a straddler the module is the one the rules chose; phase 3
splits the sim effect from the drawing (the C3 A/B hooks already give each side's vectors).

`mainloop_phase` lists the phases whose call sites reach the function (`+boot` when the boot walk reaches it too, `indirect` when
neither does). The call graph is static: calls through vtables or stored pointers are not edges, so indirect-only callees are
assigned by tag and objects, never by reach.

Phase 3 child suggestion (`simcore_order.tsv`, columns `phase3_child`, `child_basis`): first name patterns (3.4 shot|ball|trial|swing;
3.5 story|event|message|news|thought|comment|bulletin|patron|announce; 3.6 walker|animal|ambient|habitat|spawn; 3.1 tile|terrain|course|cell|
height|grid|build|edit|hole|record|object; 3.2 cash|financ|member|price|rating|award|year|date|score|employee; 3.3 golfer|pair|arriv|mood|
skill; 3.7 tick|command|frame|pump), then the subsystem tag, then the strict majority of already assigned callers and callees (four
passes), then `3.0` for call-free leaves; the rest is `unassigned`. This is a hint for the child briefs, not a decision.
