// Batch c3dll fixtures (jgld.dll pilot). Layouts from re/match/jgld_list.cpp.
Object.assign(globalThis.DIFF_FIXTURES, {
  // a circular list of five ListNodes (prev/next links both ways), head = node 0, current = node 2; data 0x100*(i+1),
  // flag i+1. A second LinkedList with no head. The vtables are never called by find, so they stay null.
  c3dll_list() {
    const nodes = [];
    for (let i = 0; i < 5; i++) nodes.push(Memory.alloc(0x14));
    nodes.forEach((n, i) => {
      n.writePointer(ptr(0));
      n.add(4).writePointer(nodes[(i + 4) % 5]);      // prev
      n.add(8).writePointer(nodes[(i + 1) % 5]);      // next
      n.add(0xc).writePointer(ptr(0x100 * (i + 1)));  // data
      n.add(0x10).writeU8(i + 1);                     // flag
    });
    const list = Memory.alloc(0x1c), empty = Memory.alloc(0x1c);
    list.writePointer(ptr(0)); list.add(4).writePointer(ptr(0x77)); list.add(8).writeU8(0x55);
    list.add(0xc).writeS32(5); list.add(0x10).writePointer(nodes[0]); list.add(0x14).writePointer(nodes[2]);
    list.add(0x18).writeS32(0);
    empty.writePointer(ptr(0)); empty.add(4).writePointer(ptr(0x88)); empty.add(8).writeU8(0x66);
    empty.add(0xc).writeS32(0); empty.add(0x10).writePointer(ptr(0)); empty.add(0x14).writePointer(ptr(0));
    return { list, empty, n0: nodes[0] };
  },
});
