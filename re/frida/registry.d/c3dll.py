# Batch c3dll: jgld.dll pilot (module-aware hooks; addr is the RVA).
# LinkedList::find over a five-node circular list (data 0x100..0x500, flags 1..5) and an empty list: a hit on each
# node (each sets a different current node, data and flag), misses (no write), and the head-null early return.
HOOKS = {
    "LinkedList::find": dict(module="jgld.dll", addr=0x000072B0, abi="thiscall", ret="int", args=["pointer", "pointer"],
                             fixture="c3dll_list", state=[("$list", 0, 0x1c), ("$empty", 0, 0x1c)],
                             vectors=[("$list", d) for d in (0x100, 0x200, 0x300, 0x400, 0x500, 0, 0x150, 0x600, 1)]
                                     + [("$empty", d) for d in (0x100, 0, 0x300)]),
}
