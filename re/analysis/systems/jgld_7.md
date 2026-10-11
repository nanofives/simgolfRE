# jgld_7: jgld's C++ dynamic-initializer stubs (2026-10-11)

Named for round 27 (batch c3dn), whose four constructors (constructTrackedList, Array_e0/Rec14/Rec94::ctor) had only
these unnamed stubs as callers. jgld's CRT initializer table runs from __xc_a 0x10122000 to __xc_z 0x1012221c; __cinit
0x1007f160 loads both bounds (0x1007f18a / 0x1007f185) and calls every non-NULL slot. Slots 65 and 67-69 (0x10122104,
0x1012210c, 0x10122110, 0x10122114) hold `dynInit_trackedList` 0x10006970 and the three `dynInit_*` stubs for the global
arrays g_arrayE0 0x101286e0, g_modes 0x101286c8 and g_devmodes 0x101286f8. Each array stub calls a `construct*` helper
(ecx = the global, then the Array constructor through its incremental-link thunk) and a second helper (0x10065050,
0x10065160, 0x10065270, not named here). Slot 66 (0x10122108) holds 0x100074b0, not named here.
