# exe_7: the exe CRT's delete wrapper (hand-named, 2026-10-09)

`operatorDelete_4a4ffc` 0x004a4ffc is 11 bytes: `push [esp+4]; call _free 0x004a5007; pop ecx; ret`. About 80 functions
call it, nearly all scalar or deleting destructors and EH unwind funclets, which is the operator delete call pattern
of VC6 code; the library symbol itself is not determined because FidDb gave it no name. Named because
NodeList::clear 0x00487310 (round 21 heap pilot c3cn) was refused by the gate with this as its only callee.
