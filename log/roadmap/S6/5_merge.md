# S6.5 merge

## re/port/defect_profiles.tsv rows (switch, value 0, decision_id, reason), profile `fixed`
replaceInText.tailLimit	0	D46	a tail of 0x200+ characters overflows the original's stack buffer and corrupts its return address; fixed refuses the replacement and leaves the text untouched
generateRandomCourse.searchBound	0	D52b1	the type-0x11 search (layout kind 1) never ends when the window x 10..39, y 0..29 holds no such cell; fixed skips that feature
generateRandomCourse.searchBound	0	D52b2	the clubhouse search never ends when the window 15..31 holds no cell of the base type that evalPlacementArea accepts; fixed takes the window centre
Note: both D52b rows share one switch (D52b3 and D52a stay `ask` and are NOT covered by it; D52b3 still raises NoReturn at value 0).

## Other
- No modules.inc / CMake change. `ev::replace_with` gained a defaulted `limit_tail` parameter in port/src/sim/events_hook.h (callers unchanged).
- generateRandomCourse is also called through the registry from course.cpp (switchToCourse) and tick_menu.inc; they follow the switch automatically.
