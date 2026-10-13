# exe_14: round 33 callee names (2026-10-13)

Callees the round 33 batches (re/C3_ENDGAME_PLAN.md path A) needed named before their callers can pass the C3 gate,
proposed in log/c3/c3ez_names.tsv, c3fc_names.tsv and c3fd_names.tsv with the instruction evidence in each row.

## Window helpers (batch c3ez)
- `Window::passLocalRect` 0x0047b9a0 and `return0Pop4_487b00` 0x00487b00: the callees that blocked Window::setSize
  0x0047b4e0.
- `Window::blitToTarget` 0x00480dc0: the code at 0x00480dc0 holds `call 0x475c20` (0x480f30), the one caller of
  DC_calla0 0x00475c20; it was outside every function of the master until created with CreateFunctions.java.

## Static CRT (batch c3fc)
`_fopen` 0x004a614d, `_fclose` 0x004a609f, `_fgets` 0x004a65ee, `_strtok` 0x004a678b, `_strstr` 0x004a64f0, `__itoa`
0x004ad425: FID names of the static CRT that loadGolferRoster and loadSoundTable call; they had no hooks.csv row.

## Sound-library hooks (batch c3fd)
`openHookA` 0x004873f0, `openHookB` 0x004879b0, `openHookC` 0x00487b90: the exe code that calls sound.dll's
Dll_Wave_Device / Dll_Midi_Device / Dll_Wave_In_Device::create_device through the slots soundVersionCheck 0x00483fc0
fills with GetProcAddress; outside every function of the master until created with CreateFunctions.java.
