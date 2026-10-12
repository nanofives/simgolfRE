# sound_14: the two withdrawn round-1 names, renamed from their vtable slots (2026-10-12)

The round-1 names stopEngineVoices 0x1002f680 and createBlankVoice 0x1002fc10 were withdrawn on 2026-10-06
(re/names/withdrawn.tsv: their cited MessageBoxA import and string 0x10063e98 are not referenced by either body), which
left both rows at C0. Neither has a direct caller; `re/tools/pointer_callers.py` finds each as a vtable slot.

`Ch5c2ec_forwardSlot37` 0x1002f680 is slot 37 (+0x94) of vtbl_Ch5c2ec 0x1005c2ec. It loads this+0x40 (0x1002f680) and
tail-jumps to slot +0x94 of that object (0x1002f689), or returns 0 when the pointer is NULL (0x1002f68f). What the
forwarded slot does is not determined here; the name records only the forwarding.

`enqueueWaveCmd10` 0x1002fc10 is slot 10 (+0x28) of vtbl_Ch5c3a4 0x1005c3a4. It has the shape of enqueueWaveCmdE
0x1002fb70 with command code 0x10 instead of 0xe: a 0x10-byte node from operator_new, pairStore(node, 0x10, this) and
WaveCmdQueue::pushValue on g_soundEngine 0x100b4a20. Before allocating it stores the address this+0xf8 into +0x1414 of
the object this+0xf8 points to (0x1002fc57). It returns 1 when g_soundEngine is NULL and 0 otherwise.
