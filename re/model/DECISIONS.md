# Phase 1 model decisions taken by the parent

- 2026-10-18, class name of the vtable at 0x004bb3f8 (106 slots): `TextView`. Streams E1 and E2 disagreed (E2 `Dialog`). Evidence for TextView:
  its constructor is `TextView::ctor` (the only constructor storing the vtable), and 21 names in re/names already use the TextView prefix.
  `Dialog::dtor` 0x00491710 and its thunks in re/names carry the older prefix: stream G checks whether to rename them
  (`TextView::dtor` ...) and applies it with the other names; the model uses TextView everywhere.
- 2026-10-18, ownership overrides from the stream reports: 0x56c7e4 / 0x5a6378 (.sve 50x50 grids), animals 0x56d1b8, the tile-type attribute
  table 0x578350.., the corner-level grid 0x5a4998 (cluster 1034) and the noise source 0x838aa0 (cluster 1640) belong to stream C.

## 2026-10-18, stream G1 (static integration): merge decisions

Applied by `re/tools/model_merge.py` (RESOLUTIONS, G_ROWS, MARKS, CANONICAL_TYPE); evidence per row in re/model/addresses.tsv.

- **One header, generated.** re/model/include/simgolf_model.h is generated from the six sections (order f, c, b, d, e2, e1;
  no section uses another's types). The generator rewrites the instruction / data addresses in the sections' comments
  (e2.h had 189, d.h 1), so the header the port ships holds none (decision 2); the sections keep their evidence.
  `tests/test_model_header.py` fails when the header is stale.
- **Duplicate types: E2 / E1 class names are canonical.** D's SgSnd = Snd, SgSurfaceObj = SpriteRef, SgFlicObj = Flic,
  SgFlicStream = FlicFrame, SgCache58 = PaletteCache, SgSndGroupDevice = NodeList, SgSndGroup = NodeListBucket,
  SgCursorObj = SgCursor (E1); E1's opaque Image bytes = E2 Image. The section names stay (no rename inside the
  streams' tables); the header's integration block asserts every pair has the same size and member offsets, and the
  address map's `c_type` column carries the canonical name.
- **NodeListBucket (e2.h) fixed:** its element constructor FUN_004877d0 stores byte [+0] = 1, dword [+4] = 0x7f and the
  vtable 0x004bafac at +8 (0x004877d4..0x004877ee); E2 had the vtable at +0. The record is D's group record (enabled,
  muted, gain) with a sub-object vtable at +8.
- **Map symbol = names label.** Where a names file labels an object's start, that label is the map symbol (C's
  snake_case symbols become its own g_* names, D's g_sound_device_music becomes g_musicDevice); the stream symbol stays
  in `stream_symbol`.
- **Overlaps resolved** (each a RESOLUTIONS row): D's five building-name rows are F's F_BuildItem records 7, 9, 12, 14
  (+0x00) and 19 (+0x12) (table base 0x4c26b0, not +4 of a record at 0x4c2738); D's single Surfaces 0x587fdc / 0x58baa0 /
  0x5a57e8 are elements 7 / 24 / 24 of SpriteRef arrays; D's duplicate g_surfaces5678b8 is dropped for E1's; D's credits
  pointers start at 0x83b164 (2 pointers; 0x83b15c / 0x83b160 are Reader +0x15c / +0x160); D's 0x519a20 is a FontRef
  (0x20, C4837f0::ctor from 0x00404700, vtable 0x4ba468 at 0x00404720), not a 0x2c Surface; D's Cursor object is 0x14
  bytes (Cursor::ctor writes +0..+0x10, 0x00488494..0x004884a3); B's two save dwords 0x4c284c / 0x4c2850 are elements of
  F's int32[4]; F's tail-A buffers are bytes of golfer-type record 76; E1's g_ui_4c79ea is entries of F's point table.
- **F's 0x4c1a40 retyped**: not a 5-byte string but the file image of the tile-type attribute table, TileTypeAttr[23],
  copied by `rep movsd` of 0x114 dwords to 0x578350 (0x0040f5f7, 0x00440f9a, 0x004411d6, 0x004412cf, 0x004414a6); renamed
  f_tab_tileTypeAttrInit_4c1a40.
- **E2's vbtable 0x4ba28c has 2 entries** (0, 0x14), not 6: the dwords after it are 12 zero bytes and the double 1.0 at
  0x4ba2a0 that allocResampleTable reads (0x00481906 / 0x00481919 / 0x00481931); 4 rows removed from e2_vtables.tsv.
- **Ownership settled**: the four .sve 50 x 50 grids and the corner-level grid stay C (parent's override), the
  money-popup ring x / y (C cluster 647's 0x542fd8 / 0x542ff8) is typed by G1 next to E's value / time arrays,
  g_holesPlusOne 0x5685f0 stays E2's row (C documents it), cluster 1598 needs no split (every address is in an E1 or E2
  row, no overlap). The 20 X clusters are typed by G1 (15 rows) or marked (biased base / loop end).
- **Names**: the names files stay the single source; withdrawn data labels are listed in re/names/withdrawn.tsv (ApplyNames
  resets only functions there: the later Ghidra step deletes the data labels with DeleteLabels.java). Interior labels
  whose declared type claims a whole table (g_animals, g_typeAttr, g_recordOccupancy) are withdrawn; field / element
  labels whose type is the field (gThoughtTable, g_golferTypeSlot, g_polyVerts, g_polyVertCount, g_playerSkillMask,
  g_resCloseHook1..3, g_objectOfferCount, g_greensFee) stay. Function class prefixes follow the model's classes:
  Dialog:: -> TextView:: / CompositeView::, View4804a0 / Base4805a0 -> Window, Cache48xxxx / Table483 -> PaletteCache,
  C4837f0 / the FontRef `Surface::` methods -> FontRef, Decoder482ae0 / 482b20 -> S3ListNode / ListNode10,
  HashTable::bucketEmpty / bucketSet -> SndGroup_isDisabled / SndGroup_isMuted (re/analysis/data/model_name_changes.md).
- **Save fields 25, 29, 46, 76, 77, 78 named from G2's proposals** (re/analysis/data/sve_field_status.md 3, each re-read at
  its instructions): g_courseReportScore 0x561250 (store 0x00452dc1), g_buildingUnlockLevel 0x5a6364 (0x0040fbe7,
  0x0040efef..0x0040eff0, bound at 0x004342ef), g_proChallenger 0x4c2e14 (store 0x0041037d; F's row cut to one int32: no
  literal 0x4c2e18 in the image), g_namePool / g_namePoolOffset / g_namePoolLength 0x56fcb0 / 0x59d81c / 0x5a46b8 (one
  string pool: strlen + 1 stored at 0x0045b9ca, end = offset + length at 0x0045b949..0x0045b954). The old labels
  gTextBuffer / gTextBuffer2 and the E2 / D / F placeholders are replaced.

## 2026-10-18, corrections reported by stream H, to apply at the start of the next phase
- vtables 0x004ba2d8 and 0x004ba468: the classes are `SpriteRef` (0x2c bytes) and `FontRef` (0x20 bytes) (e2_classes.tsv and their installers); e2_vtables.tsv and the merged map still carry `GfxSurface` / `SurfaceRef`.
- e1_vtables.tsv types vtable 0x004bc3b4 as `SgRecList`: its installer is `panelSubCtor4a3110` and slot 0 is `recordCopyStr20`, so the class name is not supported by the slots.
- e1_vtables.tsv has no rows for table 0x004ba2a8 (stored as a vtable by `deletingDtor_adj1c_402790`).
- `g_ui83aa9c` (0x0083aa9c) is a pointer to the root window object (the model types it int32_t[2]); 0x0083ab20 is the pointer-capture window.
- The 16-record table (0x58 bytes each) at pointer 0x0083ff6c that `netMarkRecordCb` scans is untyped.
- `g_ui_567afc` (0x00567afc) is a panel-mode word (3 shot style, 2 golfer list, 1 hole list, otherwise theme slot); `f_w_4c2848` and 0x004c2854 are the theme-slot and hole selections; `g_value56199c` bounds `g_panelListIds`.
- `g_titleBarH` (0x0083ff10) is stored once at startup by `initHscrollHeight` 0x00496020 as GetSystemMetrics(3), the horizontal scrollbar height.
- Snd vtable slots 41-45 point at constant-0xb stubs (unimplemented slots, not device forwarders); `Snd+0x40` and `VoiceRx+0x40` hold the sound device pointer, `NodeList+0x14` the child pointer.
