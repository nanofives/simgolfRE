# exe_11: path search and animal globals (2026-10-11)

Globals named from the simulation plan's phase 3 batch c3eb (pathStep 0x0042e7e0, updateWalkers 0x00402a40, placeAnimal
0x004017d0), all at C3. Rows: `re/names/exe_11_globals.tsv`.

## Path search
pathStep 0x0042e7e0 runs a wave search over the 50 x 50 tile grid. `g_pathGrid` 0x0058d370 (byte per cell, index
x * 50 + y) is cleared at 0x0042e7f3..0x0042e7ff, the start cell set to 1 at 0x0042e93f, and cell costs stored at
0x0042eb10. The wave queue is two short arrays of 0x400 entries: `g_pathQueueX` 0x005a7404 (store 0x0042eb16, read
0x0042e972) and `g_pathQueueY` 0x005a8028 (store 0x0042eb1e, read 0x0042e97a); the queue index wraps at 0x0042e987 and
0x0042eb27. pathStep moves nothing itself; it draws only when bit 0x1000 of the flag word is set.

## Animals
`g_animals` 0x0056d1d8 holds 100 records of 0x3c bytes (type, map, tile x / y at +6 / +8, wait +0xa, state +0xc, sub-tile
position +0x10 / +0x14, heading +0x18, steps +0x19, frame +0x1a). placeAnimal 0x004017d0 writes the cell it computes into
`g_animalCell` 0x004e9a80 (0x004018c8) before passing it to enqueueSprite, and formats numbers with itoa into
`g_itoaBuf4e9a70` 0x004e9a70 (0x0040192a, 0x00401997) before appending them to the text buffer 0x0051a068.
