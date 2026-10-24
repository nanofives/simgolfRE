# S6.4 merge notes

No shared file needs an edit: the golfers module already is in modules.inc and its ModuleDef now carries four DefectDefs.
No copy of these bodies exists elsewhere: tick_frame.inc (cc("spawnObject"), cc("loadGolfPro"), cc("placeHabitat")) and
menu.cpp (call_peer "loadGolfPro") call through the registry; placeAnimal is reached through tickAnimals in golfers_world.inc.

## Rows for re/port/defect_profiles.tsv (switch, value 0, decision_id, reason)
placeHabitat.clusterLoopBound	0	D8,D40a,D40b	slot 127 with the byte after the table 0xff is treated as taken: the cluster loop is not entered, the function continues to its message and sound as it does when that byte is not 0xff; the second addHabitatSlot no longer writes the link word before the table
spawnObject.cellSearchBound	0	D41	no free cell in the bank's 4 x 4 corner: return before the animal record is claimed and before any RNG draw (the same result as "no animal record free", which the function already has)
loadGolfPro.divideAndRejectGuard	0	D42a,D42b	a zero divisor (difficulty -4 for the first idiv, 4 for the second) or an empty pro roster 1..99 ends the draw with pick 0 (the arm a repeated pick already takes); taken[] is not marked and the golfer is built from pro record 0
placeAnimal.zeroFrameCount	0	D43	sprite frame count 0: return before the idiv; nothing has been written by then
