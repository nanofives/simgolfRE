# undisc_sound: sound.dll code Ghidra never made into functions (2026-10-09)

29 of the 236 entries that `re/tools/undiscovered.py` found and `ghidra/scripts/CreateFunctions.java` created
(log/undisc_created.tsv) are in sound.dll. All 29 are named here; none of them is a false entry.

## 1. Device-class vtable slots (14 rows)

`re/tools/pointer_callers.py` resolves every reference to the constructor that stores the table, which names the
class:

| vtable | installed by | class |
|---|---|---|
| 0x1005b000 | Sound::ctor 0x10003f30 | Sound |
| 0x1005b28c | Midi_Device::ctor 0x10009be0 | Midi_Device |
| 0x1005b304 | Wave_Device::ctor 0x1000b1d0 | Wave_Device |
| 0x1005b3d4 | Wave_In_Device::ctor 0x1000e870 | Wave_In_Device |
| 0x1005b6a8 | Seq_ctor 0x1001f6f0 | Seq |
| 0x1005c450 | Sound5c450_ctor 0x10030080 | Sound5c450 |
| 0x1005c7e8 | MultiSound_ctor 0x10039690 | MultiSound |

The five `Wave_Device` slots are the thin guards over the module globals: 0x100b49f0 (the DirectSound device,
`setCooperativeLevel`, `caps`, `deviceName`), 0x100b49f4 (the stream, `closeStream`) and the full
`Wave_Device::open` 0x1000b420, which is the 1,067-byte routine that builds the device, the stream, the primary
buffer and the two 3D buffers, places the listener, starts the signal thread and registers with the sound timer.
`Wave_In_Device::start` / `stop` guard 0x100b4a04 the same way.

`Midi_Device::shutdown` 0x10009e40 is slot 5 of the Midi_Device table: it kills the timer event, walks the
client list `g_soundRegistry` 0x100b5000 deleting each node and calling the client's virtual slot 0 with 1,
calls `midiOutClose` on the handle at `this+0x3c` and frees the command-table list `g_cmdTableHead` 0x100b4fe8.

`Sound::scaleTickRate` 0x10005d80, `Seq::loadFile` 0x10020290 and `Sound5c450::openMmio` 0x10030710 complete
the group.

## 2. The MultiSound vtable 0x1005c7e8 (10 rows)

Ten slots of the MultiSound table were missing; nine are forwarders onto the `Channel` sub-object at `this+0x70`
(`set3DPositionX/Y/Z`, `set3DBuffer`, `setReverseRate`, `setVolume`, `setRateGlide`, `applyFlags`,
`Channel_parseWaveHeader_adjPlus70` 0x10039980, which is an `add ecx, 0x70; jmp` adjustor rather than a body), and
`MultiSound::startVoice` 0x10039f30 is the one with logic: it validates bits 0 and 1 of `this+0xc8`, resets the
pan, allocates the voice with `Channel_allocAndInitVoice` 0x10001839 and posts it to `g_soundEngine` 0x100b4a20
with `WaveCmdQueue::pushNode` 0x100020b3.

Two of the setters clamp a 0..0x7f MIDI-range argument and rescale it: `setVolume` multiplies by 0x200 through
`(v + 1) << 9` for a non-zero value, and `setRateGlide` applies `(v + 1) * 0x200` to both of its endpoints while
passing 0 and -1 through unchanged.

## 3. sound.dll's own C++ dynamic initializers (6 rows)

The module has an initializer table at 0x10063104..0x10063130 (NULL-separated, run by `_initterm`). Slots 1 to 5
construct five static `Ctrl5b19c` objects with `Ctrl5b19c_ctor` 0x1000123f and register their destructors with
`atexit` 0x10042647; each takes twelve ascending constants and its own index 0..4:

| initializer | object | constants | index |
|---|---|---|---|
| 0x1001b510 | 0x100b4b50 | 2 2 4 4 5 7 7 9 9 0xb 0xb 0xc | 0 |
| 0x1001b6b0 | 0x100b4c38 | 4 4 5 5 7 9 9 0xb 0xb 0xc 0xc 0xe | 1 |
| 0x1001b710 | 0x100b4d20 | 5 5 7 7 9 0xb 0xb 0xc 0xc 0xe 0xe 0x10 | 2 |
| 0x1001b770 | 0x100b4a68 | 7 7 9 9 0xb 0xc 0xc 0xe 0xe 0x10 0x10 0x11 | 3 |
| 0x1001b7d0 | 0x100b4e08 | 9 9 0xb 0xb 0xc 0xe 0xe 0x10 0x10 0x11 0x11 0x13 | 4 |

The five rows move one step per index, which is what the index argument selects. What the constants mean is not
determined here.

The sixth row of this group is `atexitDtor_100b49b8` 0x100091f0, `mov ecx, 0x100b49b8; jmp SoundWindowObj_dtor`,
pushed at 0x100091ca inside the initializer 0x100091c0 (slot 0x10063108 of the same table) and handed to atexit.

## 4. CRT (2 rows)

- `ostream::scalarDtorAdj8` 0x10041b9d: slot 0 of 0x1005ece4, installed by `ostream_withassign` 0x10041b42 /
  0x10041bc8 and by `ostreamResetVtable` 0x10041c26. It resets the vtable, runs `~ios` 0x10041f9a, and deletes
  `this-8` (the ios base) when bit 0 of the flag byte is set.
- `__init_time` 0x1004e0d4: the init member of the LC_TIME entry of MSVC's `__lc_category` table. The table holds
  triples at 0x10067d10, 0x10067d1c, 0x10067d28 and 0x10067d34; the first is `___init_ctype` 0x1004eac1, and this
  entry is the only one that calls `__get_lc_time` 0x1004e165 and `__free_lc_time` 0x1004e4b3, which fixes it as
  the LC_TIME initializer. It swaps the 0xac-byte time table 0x100b5760 and the static pointer 0x100684b8,
  returning 0 on success and 1 when `calloc` or `__get_lc_time` fails.
