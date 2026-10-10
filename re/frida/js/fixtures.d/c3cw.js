// Fixtures for batch c3cw (round 23, jgld.dll): destructor chains. Reimplementations: shim/src/re/c3cw.cpp.
// Registry: re/frida/registry.d/c3cw.py.
//
// Every fixture installs the heap stand-in (re/frida/js/heapstandin.js) over jgld's operator new 0x1007f0f0
// and operator delete 0x1007e6d0 for the key, so the deletes in the deleting destructors and in the Array
// clears are recorded in the compared free log instead of freeing fixture memory twice.
//
// Group B (c3cw_tracked_*) also swaps jgld's tracked-object list, the module global LinkedList at 0x10128428,
// for a list of its own. log/c3/alt/c3cw_probe.py measured at the menu (hooks off, instances\c3cw): every
// entry into Tracked::ctor 0x10006ab0 (47,968), Tracked::dtor 0x10006b80 (1,388) and LinkedList::add /
// find / removeCurrent on 0x10128428 came from the process's first thread and from no other. So the fixture
// freezes that thread with the shared helper (re/frida/js/freeze.js), saves the 0x1c-byte list header and
// writes its own; the helper writes the game's header back before it resumes the thread. Everything is
// built before the freeze: with the thread down the vectors only run jgld code over fixture memory and the
// heap stand-in's recording callback.
//
// jgld.dll is relocated at run time, so every module address is built from the running base.

(function () {
  const ARENA = 0x100;     // nothing here allocates; the arena only has to exist (state: 0x110 bytes)

  function standIn() {
    return __heapStandIn({ module: 'jgld.dll', alloc: [0x7f0f0], free: [0x7e6d0], arenaSize: ARENA, fill: 0xcd });
  }

  const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

  // ------------------------------------------------------------------------------------- group A
  // One 0x2000-byte block, pattern-filled:
  //   0x0000  twelve Array<e0> headers, stride 0x40      $e0..$e11
  //   0x0300  twelve Array<Rec14> headers, stride 0x40   $r0..$r11
  //   0x0600  twelve Array<Rec94> headers, stride 0x40   $s0..$s11
  //   0x0900  twelve FontBase objects, stride 0x24       $f0..$f11 (odd stride: the store lands on dwords
  //                                                      the 0x40 grid does not use)
  //   0x1000  the buffers, 0x40 apart (never really freed: operator delete only records them)
  // Header: vtable +0, buffer +4, capacity +8, count +0xc, growth step +0x10 (the five fields Array<T>::clear
  // 0x10068000 writes at 0x1006803b-0x10068059). Vectors 3, 6, 9 and 11 have a NULL buffer, so the clear's
  // `je` at 0x10068020 skips operator delete for them.
  function arrays() {
    const h = standIn();
    const blk = __keepAlloc(0x2000);
    fill(blk, 0x2000, 0x3c77);
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk };
    const nobuf = [3, 6, 9, 11];
    [['e', 0x0000], ['r', 0x0300], ['s', 0x0600]].forEach(function (t, k) {
      for (let i = 0; i < 12; i++) {
        const o = blk.add(t[1] + i * 0x40);
        const buf = blk.add(0x1000 + (k * 12 + i) * 0x40);
        o.add(4).writePointer(nobuf.indexOf(i) >= 0 ? ptr(0) : buf);
        o.add(8).writeS32(i + 2);
        o.add(0xc).writeS32(i);
        o.add(0x10).writeS32(3 * i + 1);
        out[t[0] + i] = o;
      }
    });
    for (let i = 0; i < 12; i++) out['f' + i] = blk.add(0x900 + i * 0x24);
    return out;
  }

  // ------------------------------------------------------------------------------------- group B
  // One 0x6800-byte block, pattern-filled:
  //   0x0000  twelve objects $o0..$o11, stride 0x840: big enough for a Palette, whose destructor reads +0x80c
  //           and Palette::reset 0x1006a380 writes +0x810 and +0x814. Even objects have +0x80c pointing at
  //           their own sub-object (so 0x1006a317 clears its +4), odd ones have it NULL (je 0x1006a30c).
  //   0x6400  up to eight list nodes, stride 0x20: ListNode {vtable +0, prev +4, next +8, data +0xc, flag
  //           +0x10}; +0x14 receives the delete stub's mark
  //   0x6600  twelve 0x10-byte sub-objects for +0x80c
  //   0x6700  the delete stub's counter
  // Node vtable slot 0 is a __thiscall(int) stub (removeCurrent deletes a node through it with 1, at
  // 0x1000715d / 0x10007224): `eax = ++*ctr; node[+0x14] = eax; ret 4` (c3bw's and c3cn's stub), so which node
  // was deleted, and in what order, is compared state and nothing reaches the CRT.
  //
  // shape: 'empty' | 'one' | 'five' | 'fivehead' (see re/frida/registry.d/c3cw.py).
  function tracked(shape) {
    const base = Process.getModuleByName('jgld.dll').base;
    const glist = base.add(0x128428);
    // Settle guard: a callees-off boot whose first freezing key ran 2 s after the window (inside the start-up
    // burst of Tracked::ctor, ~3,000 jgld operator new calls a second through the stand-in's forwarding path)
    // hung the game. Wait until the list's count (+0xc) has not moved for 0.5 s before installing anything.
    let last = glist.add(0xc).readS32(), still = 0;
    for (let t = 0; still < 5; t++) {
      if (t >= 600) throw new Error('c3cw: tracked list still changing after 60 s; not freezing');
      Thread.sleep(0.1);
      const c = glist.add(0xc).readS32();
      still = c === last ? still + 1 : 0;
      last = c;
    }
    const h = standIn();
    const blk = __keepAlloc(0x6800);
    fill(blk, 0x6800, 0x7a3e);
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk, glist: glist };

    const ctr = blk.add(0x6700);
    ctr.writeU32(0);
    const page = Process.pageSize;
    const code = __keepAlloc(page);
    Memory.protect(code, page, 'rwx');
    code.writeByteArray([0xA1, 0, 0, 0, 0, 0x40, 0xA3, 0, 0, 0, 0, 0x89, 0x41, 0x14, 0xC2, 0x04, 0x00]);
    code.add(1).writePointer(ctr);
    code.add(7).writePointer(ctr);
    const nodeVt = __keepAlloc(0x20);
    for (let i = 0; i < 8; i++) nodeVt.add(i * 4).writePointer(code);

    for (let i = 0; i < 12; i++) {
      const o = blk.add(i * 0x840);
      const sub = blk.add(0x6600 + i * 0x10);
      o.add(0x80c).writePointer(i % 2 === 0 ? sub : ptr(0));
      out['o' + i] = o;
    }

    const n = { empty: 0, one: 1, five: 5, fivehead: 5 }[shape];
    const nodes = [];
    for (let k = 0; k < n; k++) {
      const p = blk.add(0x6400 + k * 0x20);
      p.writePointer(nodeVt);
      p.add(0xc).writePointer(out['o' + k]);       // data: the tracked object
      p.add(0x10).writeU8(0x40 + k);               // flag byte, copied to the list's +8 by find/removeCurrent
      nodes.push(p);
    }
    for (let k = 0; k < n; k++) {
      nodes[k].add(4).writePointer(nodes[(k + n - 1) % n]);   // prev
      nodes[k].add(8).writePointer(nodes[(k + 1) % n]);       // next
    }
    const head = n ? nodes[0] : ptr(0);
    const cur = shape === 'five' ? nodes[2] : head;

    // Freeze the game's first thread, then swap the list header. The header's vtable dword (+0) is left
    // as the game's: neither find nor removeCurrent reads it.
    __freeze.thread();
    __freeze.save(glist, 0x1c);
    glist.add(4).writeU32(0x5a5a0001);             // m_data
    glist.add(8).writeU32(0x5a5a0002);             // m_flag (byte) and the three bytes after it
    glist.add(0xc).writeS32(n);                    // m_count
    glist.add(0x10).writePointer(head);            // m_head
    glist.add(0x14).writePointer(cur);             // m_cur
    glist.add(0x18).writeU32(0);                   // m_ordered
    return out;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3cw_arrays() { return arrays(); },
    c3cw_tracked_empty() { return tracked('empty'); },
    c3cw_tracked_one() { return tracked('one'); },
    c3cw_tracked_five() { return tracked('five'); },
    c3cw_tracked_fivehead() { return tracked('fivehead'); },
  });
})();
