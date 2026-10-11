# sound_13: three small callees of Wave_In_Device wrappers (2026-10-11)

Named for round 28 (batch c3dt), whose serviceWaveDevice 0x1000e310 and Wave_In_Device::op1/op2 0x1000ed70/0x1000eda0
had them as their only callees. `return0Pop4` 0x10012040 is `xor eax, eax; ret 4`, so serviceWaveDevice always returns 0
on that path. `setField6cReturn0` 0x10039450 stores its argument at this+0x6c and `clearField6cReturn0` 0x10039470
clears it; both return 0. c3dt proposed Wave_In_Device::setStream / clearStream for op1 / op2 (the field at +0x6c), which
are applied with the round's renames.
