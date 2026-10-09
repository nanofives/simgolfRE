# sound_11: three channel vtable slots (2026-10-09)

Named for round 23 (batch c3cx): slots 16, 28 and 39 of the channel vtables 0x1005bb90 and 0x1005bcb4 (installed by
SubChannel_ctor 0x1002b4e0 and Ch5bb90_ctor 0x1002c490). MultiSound overrides each slot and calls the channel body:

- `Channel_setField4Masked7f` 0x1002c370 (slot 16): stores value & 0x7f at +4; MultiSound::setVolume 0x1003a0d0 calls it.
- `Channel_collectFlagBits` 0x1002c0d0 (slot 28): builds a status word from the sub-object at +0x40, the dword at +0x30
  and the low byte of +0x58; MultiSound_getStatus 0x1003a560 calls it.
- `Channel_setField5cClamped1200` 0x1002bff0 (slot 39): clamps to -1200..1200, stores at +0x5c, forwards to the
  sub-object's slot; MultiSound_setPitchCents 0x1003a210 calls it.

The names describe the stores; what each field means is not determined by these bodies.
