# exe_8: slot-60 bodies of the view classes (2026-10-09)

Named for round 23 (batch c3cu), whose `panelSubSlot60_vtordisp` 0x004a4a70 needed its tail-jump target named for the
gate's anti-island callee check. `viewDispatchSlot60` 0x004a0ff0, one of the two-level switch dispatchers of
`viewCtor4a0740`'s class (re/analysis/systems/undisc2_exe.md), routes slot 60 by the flag word at [ecx-8] to four bodies:

| tag | adjust | body | what it does |
|---|---|---|---|
| 8 | this-0x8c | panelSubSlot60 0x004a4870 | calls slot +0x120 of its virtual base (vbtable at this-0xe0, entry +4) |
| 0x10 | this-0x180 | listPanelSlot60 0x0049e7b0 | same through the vbtable at this-0x24 |
| 1 | this-0x168 | optionDialogSlot60 0x0049fd80 | same through the vbtable at this-0x1c |
| 2 | this-0x1a0 | ListBox::Slot60 0x00487af0 | the single instruction `ret 4` |

Each body ignores its stack argument. What slot +0x120 of the virtual base does is not determined by these bodies.
