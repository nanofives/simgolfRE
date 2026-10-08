# jgld_4: the tracked-list static initializer (hand-named, 2026-10-08)

`constructTrackedList` 0x100069b0 (jgld.dll, debug build, matched 100% in `re/match/jgld_list.cpp` as the
compiler-generated `_$E1`; `_$E2` compiles to the same bytes there, so the compiler's own name is not determined)
constructs the module global `g_trackedList` 0x10128428 by calling `LinkedList::ctor` 0x10006c80. Its only caller is
the initializer 0x10006970 (`_$E4`), which also runs the atexit registration 0x100069f0 (`_$E3`) of the destroy stub
`atexitDestroyGlobalList` 0x10006a40. Named because LinkedList::ctor was held at C3 with this as its only caller
(log/c3/c3bw_notes.txt).
