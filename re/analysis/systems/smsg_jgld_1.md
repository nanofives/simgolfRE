# smsg_jgld_1: jgld.dll names from SMSG-decomp's PDB, checked against our disassembly (S10.9, 2026-10-29)

Source of the names: third_party/smsg/release/jgld.pdb (log/roadmap/S10/pdb/jgld.procs.tsv). The rules are the ones in
smsg_sound_1.md, applied by hand: jgld.dll is a debug build (/Od /ZI /GZ), so the body-shape classifier, which reads
release code, does not apply. Every row has a hand verdict in log/roadmap/S10/9_manual_jgld.tsv.

Where our files already name the class, rows keep our class name and the evidence says what SMSG calls it:
- SMSG's JGLSys vtable at 0x1011d640 is our `Display` (installer Display::Display).
- JGLSprite is our `Sprite`, JGLPalette our `Palette`, PRNG our `Random`, Matrix4<float> our `Matrix`.

The message-pump slots 72..84 of the Display vtable are named by the PeekMessageA filter range each one passes:
- do_draw: WM_PAINT 0xf.
- do_key and flush_keyboard: 0x100..0x108, the keyboard range.
- do_char: WM_CHAR 0x102.
- do_mouse_move: WM_MOUSEMOVE 0x200.
- do_mouse and flush_mouse: 0x200..0x20a, the mouse range.

do_timer and flush_timer filter on 0x401, and no jgld code posts that message, so both are rejected. GraphSys
(DisplayBase) slots 16..71 are constant stubs (`mov eax, 0x18`), also rejected.

Candidates: 63 game functions, all with a hand verdict: 22 accepted, 41 rejected. The 57 library candidates are in
smsg_jgld_crt.md.
