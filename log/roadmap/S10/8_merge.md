# S10.8 merge list (for the S10 lead / orchestrator)

S10.8 runs no tracker writes, no ApplyNames and no re/model edits. Everything below is for the lead to apply.

## 1. New names file (already on disk, gates pass)

- re/names/smsg_exe_1.tsv (42 rows) + re/analysis/systems/smsg_exe_1.md.
- Tracker: `~/bin/py -3.12 scripts/re_classify.py batch re/names/smsg_exe_1.tsv --to C1`, then
  `~/bin/py -3.12 re/tools/c2_note.py --all --out log/roadmap/S10/8_c2.tsv` + `re_classify.py batch log/roadmap/S10/8_c2.tsv --to C2`.
  Bodies of 5 bytes or less stay C1, as in stream H.
- Ghidra: ApplyNames on golf_clean.exe (`ghidra/scripts/ApplyNames.java re/names`, no overwrite needed: all 42 are FUN_ in the master).
- Hand-over to goal 1 (S10.1): the 24 libjpeg pairs are MATCH lines for the existing wrappers. 22 are at 100%, see
  log/roadmap/S10/8_jpeg_autoname.log. jpeg_fdct_ifast / _float need a jfdct wrapper. rgb_ycc_start (U-1886) needs match.py
  not to mask the immediate 0x807fff.

## 2. Renames of existing rows (file, addr, old, new, evidence)

Apply by editing the row's name in its file, then ApplyNames overwrite and `re_classify.py retag` (levels unchanged).
PORT COST: rows marked P are referenced by name in port/src, port/test, shim/src or re/frida (hooks.csv names are the port's
registry, vector and golden keys). Rename those only with a port step that adds aliases, or keep the hooks.csv name and change
only re/names + Ghidra. The lead decides. Rows marked - have no reference there.

| file | addr | old | new | port | evidence (our disassembly) |
|---|---|---|---|---|---|
| exe_0.tsv | 0x004038f0 | drawFlyingBall | drawWalkers | P (4 port, 7 shim/frida files) | the C3 Purpose already says it: the loop over the 64 walker records 0x00585850 (0x4c bytes each), pointer 0x0058586e..0x00586b6e (0x00403a8f / 0x00403ef0), plus golfer selection-0x80 when [0x004c2e10] >= 0x80 (0x00403901). The string `I'm flying!` 0x004c1564 is that carried golfer's label, not a ball. golf.pdb: RenderEmployees |
| exe_0.tsv | 0x0040e720 | announceBuilding | openHole | P (6 / 6) | the C3 Purpose: refuses a hole with no tee 0x00575ab8 / pin 0x00575ac8 (0x0040e739..0x0040e760), sets yards and par (0x0040e7de..0x0040e8c8), resets the hole statistics and posts the hole-open notice. The building text is an optional tail. golf.pdb: OpenHoleForPlay |
| exe_0.tsv | 0x0042dea0 | holeMagazineEvent | rateCourseHoles | P (6 / 7) | the C3 Purpose: the per-tick pass over the 18 hole records. It zeroes and sums g_fun_rating / g_par / g_yards (0x0042defe..0x0042df2c), sets the too-hard / too-easy bits (0x0042e085 / 0x0042e0b5) and the skill deltas, and posts the notices. The magazine events are one branch. golf.pdb: CalculateAllHoleStatistics |
| exe_0.tsv | 0x00422530 | rateTile | golferShotReach | P (8 / 10) | it takes a golfer index and returns 150..330, capped at 0x00422530. chooseShotTarget compares sqrt(dx*dx + dy*dy) at 25 per tile (0x00423162..0x00423181, stored 0x00423188) with reach(g) + reach(g, strokes + 1) (0x004231b8..0x004231ca); the port calls it `reach in yards` (port/src/sim/shots.cpp). golf.pdb: CalculateGolferMaxDistance |
| exe_1.tsv | 0x00442180 | loadInterfaceArt | (keep) | P (31 / 9) | both names are partial: the C3 Purpose has the reset (0x004421a9..0x00442320) and the art load. golf.pdb: ResetAllGameState(startFresh) |

### 2b. The 0x0056d1b8 records are celebrity vacation-home residents, not animals (settled)

The evidence is ours alone. mainLoop's block at 0x00417a5a runs over placed objects of type 5, the lots (rateLot). It draws a
celebrity c from g_celebrities 0x0055d738 (Random::range(100), 0x00417a70..0x00417a91, and it skips records whose byte +0x20
is 0xff). It builds `<kind word> <name>` + string 0x004c6444 (`has purchased a vacation home at your golf course!`, used at
0x00417b8b) into 0x0051a068. The kind word comes from the table 0x004c2ba8 indexed by the celebrity's byte +0x20 (0x00417adb /
0x00417b01). It then posts the message (startMessage 0x00417bbe) and calls spawnObject(Random::range(6), c, lot x, lot y)
(0x00417c01..0x00417c17). spawnObject copies the celebrity's kind byte (g_celebrities + 0x20, 0x0040120a) into record +0x20
(0x00401232). Its first argument selects one of the six g_recordBanks layouts (stride 0x74 = (a*8 - a)*4 + a, then shl 2, at
0x00401245..0x00401261). The record's position is a random free cell of that layout's 6x6 occupancy grid at 0x004e6d70
(0x00401264..0x004012c3). drawRecords 0x004012d0 draws a layout on a type-5 lot whose +8 is non-zero (mainLoop 0x00413f1a..
0x00413f3e). freeRecordAtPos frees the record whose +0x26 / +0x28 equal a demolished footprint's tile (caller
clearObjectFootprint 0x0040e400). golf.pdb names these SpawnHomeVIP, RemoveHomeVIPAtPosition, RenderPlot, PlaceBuilding and
ClearPlot, with FIRST_CELEBRITY 0x9c. placeAnimal queues its sprite with the tag (x + 0x9c) | 0x100 (0x004018b8 / 0x004018c2),
which drawQueuedSprite takes as a person (golfer-palette path; roadmap S3.5b).

| file | addr | old | new | port | note |
|---|---|---|---|---|---|
| model_c_globals.tsv | 0x0056d1b8 | g_animalRecords | g_vipResidents | P (13 port files) | type VipResident[100] (0x3c): +0x20 celebrity kind byte, +0x22 layout bank 0..5, +0x26 / +0x28 lot tile x / y, +0x2c state (0x140 at spawn, 0x0040125e), +0x30 / +0x34 position inside the layout |
| exe_0.tsv | 0x004011e0 | spawnObject | spawnVipResident | P (8 / 6) | above |
| exe_6.tsv | 0x004011b0 | freeRecordAtPos | removeVipResidentAt | P (6 / 7) | above |
| exe_0.tsv | 0x004017d0 | placeAnimal | updateVipResident | P (10 / 7) | moves and draws one resident |
| exe_0.tsv | 0x00401c00 | tickAnimals | updateVipResidents | P (6 / 7) | loop over the 100 records |
| exe_6.tsv | 0x00401000 | resetRecordBank | resetHomeLayout | P (1 / 7) | clears layout a of g_recordBanks |
| exe_0.tsv | 0x00401040 | placeRecord | placeHomeLayoutPart | P (4 / 7) | places one footprint of g_recordDefs into a layout |
| exe_0.tsv | 0x00401750 | initRecords | initHomeLayouts | P (6 / 10) | builds the six layouts |
| exe_0.tsv | 0x004012d0 | drawRecords | drawHomeLayout | P (4 / 6) | draws a layout on a type-5 lot |
| exe_0_globals.tsv | 0x004e6d20 | g_recordBanks | g_homeLayouts | P (15 port files with g_recordBanks / g_recordDefs) | six 0x74-byte layouts |

Model corrections: re/analysis/data/model_c.md section `Animal` (the record is a VIP resident; keep the base correction
0x56d1b8), re/model/addresses.tsv rows g_animalRecords / g_recordBanks (name and type), the type name in
re/model/include/sections/c.h, and model_name_changes.md. The habitat code (placeHabitat 0x00405e30, addHabitatSlot, the table
0x572cb0, string ` habitat` 0x004c3f9c) is animals and stays as it is.

### 2c. NodeList forwarders named after the sound.dll method they reach (all in model_h3.tsv, no port / shim reference)

Every row has the same shape: `mov ecx, [ecx + 0x14]; test ecx, ecx; je; mov eax, [ecx]; jmp [eax + S]`. The object at
this+0x14 is the Wave_Device that the export Dll_Wave_Device::create_device (pointer 0x0083af7c) writes for openHookA 0x004873f0.
Wave_Device::ctor 0x1000b1d0 stores the vtable 0x1005b304 at 0x1000b225, and slot S of that vtable (incremental-link thunk
followed) is the body named in our sound.dll rows. log/roadmap/S10/8_tools/fwd.py produced the table, log/roadmap/S10/8_fwd.tsv
holds it.

| addr | old | new | S | sound.dll body (our name) | golf.pdb |
|---|---|---|---|---|---|
| 0x00487490 | NodeList::fwdChild19 | NodeList::stopOutput | 0x4c | 0x1000bef0 Wave_Device::stopOutput | Wave_Device::suspend |
| 0x004874a0 | NodeList::fwdChild20 | NodeList::startOutput | 0x50 | 0x1000bf90 Wave_Device::startOutput | Wave_Device::restart |
| 0x004874b0 | NodeList::fwdChild14 | NodeList::serviceTick | 0x38 | 0x1000c230 Wave_Device::serviceTick | Wave_Device::update_sound |
| 0x004874d0 | NodeList::fwdChild28 | NodeList::setCooperativeLevel | 0x70 | 0x1000b140 Wave_Device::setCooperativeLevel | Wave_Device::set_hwnd |
| 0x004874f0 | NodeList::fwdChild23 | NodeList::caps | 0x5c | 0x1000e230 Wave_Device::caps | Wave_Device::get_hw_mem_size |
| 0x00487610 | NodeList::fwdChild9 | NodeList::setOutputMasterVolume | 0x24 | 0x1000e2a0 setOutputMasterVolume | Wave_Device::set_volume |
| 0x00487680 | NodeList::fwdChild21 | NodeList::openWaveStreamFile | 0x54 | 0x1000b110 openWaveStreamFile | Wave_Device::start_raw_dump |
| 0x004876a0 | NodeList::fwdChild22 | NodeList::closeStream | 0x58 | 0x1000b170 Wave_Device::closeStream | Wave_Device::stop_raw_dump |
| 0x00487810 | NodeList::fwdChild32 | NodeList::serviceWaveDevice | 0x80 | 0x1000e310 serviceWaveDevice | Wave_Device::set_eax |
| 0x00487890 | NodeList::fwdChild35 | NodeList::setListenerPosition | 0x8c | 0x1000e490 (unnamed; its only callee is Ds3DListener::setPosition 0x100135a0) | Wave_Device::set_listener_position |

Not renamed (18). Fifteen forwarders reach sound.dll bodies we have not named; those are S10.9's, and once it names them, a
re-run of fwd.py gives these renames. Two reach the stubs stubReturnBA 0x1000e350 and stubReturnBB 0x1000e570, which return a
constant, so the golf.pdb intent names (set_eax_mix, set_listener_zpos) describe nothing the body does. NodeListB::fwdChild24
(Midi_Device, vtable 0x1005b28c, stored at 0x10009af9) also reaches an unnamed body.

## 3. Golfer record model corrections (re/analysis/data/golfer_record.md, re/model, UNCERTAINTIES.md status)

| U-row | field | change | evidence |
|---|---|---|---|
| U-0010 | +0x19 | `skills` byte. Bit 0 adds 50 to golferShotReach's base (the rateTile note's +0x11 bit 0, base 0x5794c0). The high nibble is g_challengeLevel 0x0059b730, written as level << 4 + 7 when customizeCharacter sets up the challenge golfer (0x0040f437..0x0040f44c, same block as flag 0x200 at 0x0040f581). Status: resolved for the writer; what bits 1 and 2 do is not determined | 0x0040f437, 0x0040f443, 0x0040f446 |
| U-0015 | +0xbc | `bridgesSeen`, a scenic-bridge memory mask, write-only. Golfers on a water tile (type 0x11, 0x0042b2de) with tile flags TF_PATH 0x20 (0x0042b2ef) and 0x100 (0x0042b2f5) get thought 7 (emitGolferThought 0x0042b31c). buildGolferComment's kind-7 texts are the scenic-bridge lines 0x004e3040 / 0x004e3018 / 0x004e2ff0 (0x0046b60a / 0x0046b63e / 0x0046b672). Then one random bit is cleared and bit (tile flags & 7) is set (0x0042b333..0x0042b34f). A whole-.text capstone scan for the displacement 0x579574 finds only these two sites (log/roadmap/S10/8_tools/sgdis.py --grep), and golfer_record.md's record_fields.py scan found no access through a record pointer, so nothing reads it. Tile-flag bit 0x100 on water is the scenic bridge (U-0200, partly settled). Status: resolved | above |
| U-0016 | +0xfc..+0xff | padding. All 23 shipped .pro / .chr files (original\ and extracted\, 46 copies) hold zero at file offset 0x71e..0x721, that is bytes 12..15 of the 16-byte block loadThemeFile reads after 0x230 + 0x4e2 bytes (freads 0x004380db / 0x004380fc / 0x0043810b), and no instruction uses them on their own. Status: resolved | 0x0043810b |
| U-0009 | +0x10 | four bits named. 0x10 / 0x8 / 0x4: the golfer used a placed object of kind 6 / 8 / 0xa (0x0042ab2c / 0x0042abed / 0x0042ac9d), with a fee through pointsPopup 0x0040c890 added to 0x00571fd4 and thought 0x35 / 0x34 / 0x33. 0x10000: rides, because buildGolferComment picks the riding bridge text 0x004e3040 over the walking one when it is set (0x0046b5fe). Status: partial, 18 bits open (U-1887) | above |
| U-0011..U-0014, U-0012 | +0x1b, +0x1d / +0x20, +0x36, +0xae | unchanged (open). golf.pdb's names (shotDifficulty, activity with ACTIVITY_* 0 / 5..7 / 10..14 / 16..19, attitude, rapport) are recorded in 8_notes.md as leads. Our value set for +0x1d equals their constants, but the code does not show the meanings | - |

## 4. UNCERTAINTIES.md rows (text in log/roadmap/S10/8_uncertain.tsv)

Add U-1885..U-1889. Set U-0015 and U-0016 to resolved and U-0010 to resolved (writer), with the evidence in section 3. U-0009 gets
a pointer to U-1887 for the bits still open.

## 5. Counts

See log/roadmap/S10/8_notes.md section 1.
