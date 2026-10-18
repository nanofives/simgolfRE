# Phase 1 model decisions taken by the parent

- 2026-10-18, class name of the vtable at 0x004bb3f8 (106 slots): `TextView`. Streams E1 and E2 disagreed (E2 `Dialog`). Evidence for TextView:
  its constructor is `TextView::ctor` (the only constructor storing the vtable), and 21 names in re/names already use the TextView prefix.
  `Dialog::dtor` 0x00491710 and its thunks in re/names carry the older prefix: stream G checks whether to rename them
  (`TextView::dtor` ...) and applies it with the other names; the model uses TextView everywhere.
- 2026-10-18, ownership overrides from the stream reports: 0x56c7e4 / 0x5a6378 (.sve 50x50 grids), animals 0x56d1b8, the tile-type attribute
  table 0x578350.., the corner-level grid 0x5a4998 (cluster 1034) and the noise source 0x838aa0 (cluster 1640) belong to stream C.
