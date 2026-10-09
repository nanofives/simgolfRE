// Fixtures for batch c3cn (jgld.dll): the heap stand-in pilot. Every fixture here installs
// re/frida/js/heapstandin.js's __heapStandIn over jgld.dll's operator new 0x1007f0f0 and operator
// delete 0x1007e6d0 for the length of one A/B key, so the arms of a function that allocates or frees
// receive the SAME block addresses and the frees become compared state instead of a hazard.
//
// Measured before relying on it (log/c3/alt/c3cn_probe.py, instances\c3cn, 2026-10-09):
//   - at the main menu only the process's FIRST thread (the one that owns the window) enters those two
//     functions: 96,718 operator new and 3,994 operator delete entries in the first 15 s, 0 from any
//     other thread, 0 from the script thread;
//   - with a replacement of the same shape installed for 10 s, 466 first-thread calls took the
//     forwarding path (`orig(size)` through a NativeFunction of the replaced address), 0 of them
//     re-entered the callback (max depth 1) and the game kept running. Frida routes a NativeFunction
//     call on a replaced address to the original body, which is what makes the forwarding safe.
// So the stand-in does NOT need the game's thread frozen: the thread check inside __heapStandIn keeps
// every foreign call on the real allocator.
//
// jgld.dll is relocated at run time, so all module addresses are built from the running base.
// Reimplementations under test: shim/src/re/c3bb.cpp (Array<T>::add) and shim/src/re/c3cd.cpp (the four
// scalar deleting destructors); shim/src/re/c3cn.cpp holds this batch's own ones.
// Registry: re/frida/registry.d/c3cn.py.

(function () {
  // The arena and the free log are registry state regions; the sizes here and in c3cn.py must agree.
  const ARENA = 0x8000;          // usable bytes; the block is ARENA + 0x10 (cursor dword at +0)
  const LOGSZ = 0x404;           // count dword at +0, then up to 0x100 recorded pointers

  function standIn() {
    return __heapStandIn({ module: 'jgld.dll', alloc: [0x7f0f0], free: [0x7e6d0],
                           arenaSize: ARENA, fill: 0xcd });
  }

  // The wider stand-in used by the allocation and release leaves: jgld's whole CRT allocation surface.
  //   0x1007f0f0 operator new      0x1007e6d0 operator delete
  //   0x1007fb40 malloc            0x100806d0 free
  //   0x1007ffa0 calloc(num, size) - two arguments whose PRODUCT is the block size, and it zeroes, so
  //              its entry carries sizeArgs: 2 and fill: 0.
  // jalloc 0x10014a40, jfree 0x10014a90, png_malloc, png_free, zcalloc, zcfree and Array<T>::clear all
  // sit ABOVE these five entry points, so they stay original code under test while the block addresses
  // and the frees become compared state.
  function standInAll(size) {
    return __heapStandIn({ module: 'jgld.dll',
                           alloc: [0x7f0f0, 0x7fb40, { rva: 0x7ffa0, args: 2, sizeArgs: 2, fill: 0 }],
                           free: [0x7e6d0, 0x806d0],
                           arenaSize: size, fill: 0xcd });
  }

  function fill(p, n, seed) {
    for (let i = 0; i < n; i += 4) p.add(i).writeU32((seed * 0x10000 + i) >>> 0);
  }

  Object.assign(globalThis.DIFF_FIXTURES, {

    // ------------------------------------------------------------------ Array<T>::add, growth branch
    // Twelve Array<Rec14> and twelve Array<Rec94> headers (vtable +0, buffer +4, capacity +8, count
    // +0xc, growth step +0x10, read at 0x100681c3-0x10068203) laid out in one pattern-filled block.
    // Eight of each have count >= capacity, so `jge 0x100681fa` (0x100681c9) takes the growth path:
    // operator new 0x1007f0f0 at 0x1006820a comes from the arena, the CRT memcpy 0x1007f3a0 at
    // 0x1006824b copies the old buffer when it is not null (`je 0x10068253` at 0x10068234), and the old
    // buffer - including a null one - is handed to operator delete 0x1007e6d0 at 0x10068287 and lands
    // in the free log. Four keep count < capacity, so the same key also runs the room branch with an
    // empty free log.
    c3cn_arrays() {
      const h = standIn();
      const blk = __keepAlloc(0x6000);
      fill(blk, 0x6000, 0x3c3c);
      const out = { arena: h.arena, freelog: h.log, ablk: blk, obj: blk };

      // [capacity, count, growth, has buffer] per vector.
      //   0-3   count < cap: the room branch, nothing allocated and nothing freed
      //   4     cap 0, count 0, no buffer: growth with the memcpy skipped and operator delete(NULL)
      //   5-9   count == cap with a buffer: growth, memcpy of count elements, the old buffer freed
      //   10    count > cap (the jge's other equal/greater side)
      //   11    growth step 0: the new buffer is cap elements and the element is written one element
      //         past its end (0x10068259/0x10068269), which the arena records
      const plan = [[8, 0, 4, 1], [8, 3, 4, 1], [8, 7, 4, 1], [12, 5, 6, 1],
                    [0, 0, 4, 0], [1, 1, 1, 1], [2, 2, 3, 1], [4, 4, 2, 1], [3, 3, 8, 1], [6, 6, 1, 1],
                    [4, 5, 2, 1], [4, 4, 0, 1]];
      // Rec14 headers at +0 (stride 0x20) with their 0x100-byte buffers at 0x400 (the largest capacity
      // is 12, i.e. 12 * 0x14 = 0xf0); Rec94 headers at 0x200 with their 0x500-byte buffers at 0x1000
      // (the largest capacity is 8, i.e. 8 * 0x94 = 0x4a0).
      for (let i = 0; i < 12; i++) {
        const [cap, count, grow, hasbuf] = plan[i];
        const o = blk.add(i * 0x20), buf = blk.add(0x400 + i * 0x100);
        o.add(4).writePointer(hasbuf ? buf : ptr(0));
        o.add(8).writeS32(cap);
        o.add(0xc).writeS32(count);
        o.add(0x10).writeS32(grow);
        out['a14_' + i] = o;
      }
      for (let i = 0; i < 12; i++) {
        const [cap, count, grow, hasbuf] = plan[i];
        const o = blk.add(0x200 + i * 0x20), buf = blk.add(0x1000 + i * 0x500);
        o.add(4).writePointer(hasbuf ? buf : ptr(0));
        o.add(8).writeS32(cap);
        o.add(0xc).writeS32(count);
        o.add(0x10).writeS32(grow);
        out['a94_' + i] = o;
      }
      return out;
    },

    // ------------------------------------------------- the four scalar deleting destructors, bit 0 SET
    // Same objects as c3cd's fixture (Matrix 0x44 bytes, Transform 0x24, LinkedList 0x1c, ListNode
    // 0x14), rebuilt here so this batch owns its own block, with the free log in the state: every
    // vector whose flags have bit 0 set (`and eax,1` / `test eax,eax` / `je` at 0x10003628-0x1000362d
    // and the twins) must hand `this` to operator delete at 0x10003633, and that pointer is now
    // compared. Half the vectors keep bit 0 clear, so one key covers both sides of that branch and the
    // free log's count differs between vectors.
    c3cn_objects() {
      const h = standIn();
      const blk = __keepAlloc(0x4000);
      fill(blk, 0x4000, 0x5e5e);
      const out = { arena: h.arena, freelog: h.log, blk: blk, obj: blk };

      const ctr = blk.add(0x3b00);
      ctr.writeU32(0);

      // __thiscall stub(int) in slot 0 of every fixture node's vtable: LinkedList::clear 0x10006df0
      // calls it with 1 (0x10006e70, 0x10006ea8), i.e. "delete this node". It only records the order:
      //   eax = ++*ctr; node[0x14] = eax; ret 4     (c3bw's and c3cd's stub)
      const page = Process.pageSize;
      const code = __keepAlloc(page);
      Memory.protect(code, page, 'rwx');
      code.writeByteArray([0xA1, 0, 0, 0, 0, 0x40, 0xA3, 0, 0, 0, 0, 0x89, 0x41, 0x14, 0xC2, 0x04, 0x00]);
      code.add(1).writePointer(ctr);
      code.add(7).writePointer(ctr);
      const nodeVt = __keepAlloc(0x20);
      for (let i = 0; i < 8; i++) nodeVt.add(i * 4).writePointer(code);

      for (let i = 0; i < 12; i++) {
        out['m' + i] = blk.add(i * 0x60);                 // Matrix
        out['x' + i] = blk.add(0x0900 + i * 0x40);        // Transform
        out['l' + i] = blk.add(0x0f00 + i * 0x40);        // LinkedList
        out['n' + i] = blk.add(0x1500 + i * 0x40);        // ListNode
      }

      let next = 0;
      const newNode = () => {
        const p = blk.add(0x1b00 + next * 0x20);
        next += 1;
        p.writePointer(nodeVt);
        p.add(0x0c).writeU32(0x7700 + next);              // data
        p.add(0x10).writeU8(next & 0xff);                 // flag byte
        return p;
      };
      const lens = [0, 1, 2, 3, 5, 1, 2, 4, 0, 3, 6, 1];
      for (let i = 0; i < 12; i++) {
        const list = out['l' + i];
        list.add(0x0c).writeU32(lens[i]);                 // m_count
        list.add(0x18).writeU32(0);                       // m_ordered
        if (lens[i] === 0) { list.add(0x10).writePointer(ptr(0)); list.add(0x14).writePointer(ptr(0)); continue; }
        const nodes = [];
        for (let k = 0; k < lens[i]; k++) nodes.push(newNode());
        for (let k = 0; k < nodes.length; k++) {
          nodes[k].add(4).writePointer(nodes[(k + nodes.length - 1) % nodes.length]);   // prev
          nodes[k].add(8).writePointer(nodes[(k + 1) % nodes.length]);                  // next
        }
        list.add(0x10).writePointer(nodes[0]);            // m_head
        list.add(0x14).writePointer(nodes[nodes.length - 1]);                           // m_cur
      }
      return out;
    },

    // ------------------------------------------- jgld's allocation and release leaves (c3cn.cpp)
    // One block holds everything the keys of this group need, and the whole block plus the arena and
    // the free log are the compared state:
    //   0x0000  png: a 0x400 scratch "png_struct". The png memory layer only tests it against NULL
    //           (0x10078e78, 0x10078ee8), so no field of it is read; png_error 0x10078fe0, the only
    //           call that would dereference it, is unreachable here (the arena never fails).
    //   0x0800  p0..p11: twelve pointers handed to the release leaves. They are inside the fixture
    //           block, so a free that really ran would be a crash; under the stand-in it is recorded.
    //   0x1000  twelve Array<T> headers (stride 0x40) and their buffers at 0x1800 (stride 0x100).
    //   0x2800  twelve destination sprites and 0x2e00 twelve source sprites (stride 0x60), with their
    //           payloads at 0x3400 (stride 0x200) and row tables at 0x5800 (stride 0x40).
    // The marker vtable's slot 2 is Sprite::copyFrom's first call (0x1001a107): a stub that counts and
    // writes the count at sprite + 0x38, just past the last field the body touches (+0x34).
    c3cn_jgld_heap() { return c3cnHeapBlock(0x2000); },
    // png_zalloc's two-memset branch needs a block over 0x8000 bytes (the `jbe` at 0x100786ec), so
    // that one key gets a 0xa000-byte arena instead.
    c3cn_jgld_heap_big() { return c3cnHeapBlock(0xa000); },

    // --------------------------------------------- golf_clean.exe's release leaves (c3cn.cpp part 3)
    // The exe's allocation entry points are malloc 0x004a504f, operator new 0x004a57f1 and free
    // 0x004a5007; operator delete 0x004a4ffc tail-calls free (0x004a5000), so replacing free covers
    // both. The exe is the process's main module, so the stand-in's RVAs are VA - 0x00400000.
    //
    // One 0x5000-byte block holds every object, pattern-filled so an unwritten field or a store one
    // byte too wide changes its hash; the whole block, the arena and the free log are the state.
    //   0x0000 twelve Buf objects (vtable +0, flag byte +4, data +8, +0xc, +0x10, +0x14, +0x18),
    //          stride 0x40, with their data buffers at 0x0400. Six have a buffer, six do not.
    //   0x0800 twelve KeyTable objects, stride 0x140: the embedded Buf starts at +0x108
    //          (`add ecx, 0x108` at 0x00488301) and its data buffers are at 0x4400.
    //   0x1800 twelve logger objects (vtable +0, buffer +4), stride 0x20, buffers at 0x1a00.
    //   0x1c00 twelve singly linked lists (head +4, +8, count +0xc), stride 0x20, with up to four
    //          nodes each from 0x1e00 (payload +8, next +0x10) and the payloads at 0x2600.
    //   0x3000 twelve `this` objects for bufferFree (+0x18 is its guard) and 0x3200 twelve records
    //          (buffer +4), with the buffers at 0x3400.
    //   0x3600 twelve NodeLists (+8, head +0xc, +0x10, count +0x18), stride 0x20, with up to four
    //          nodes each from 0x3800 (+0, prev +4, next +8).
    //   0x4000 twelve scratch pointers for jpeg_free_large.
    c3cn_exe_heap() {
      const h = __heapStandIn({ module: 'golf_clean.exe', alloc: [0xa504f, 0xa57f1], free: [0xa5007],
                                arenaSize: 0x2000, fill: 0xcd });
      const blk = __keepAlloc(0x5000);
      fill(blk, 0x5000, 0x6e6e);
      const out = { arena: h.arena, freelog: h.log, eblk: blk, obj: h.arena };

      for (let i = 0; i < 12; i++) {
        const b = blk.add(i * 0x40), data = blk.add(0x400 + i * 0x40);
        b.add(8).writePointer(i % 2 ? data : ptr(0));
        out['bf' + i] = b;

        const kt = blk.add(0x800 + i * 0x140), ktdata = blk.add(0x4400 + i * 0x40);
        kt.add(0x108 + 8).writePointer(i % 2 ? ktdata : ptr(0));
        out['kt' + i] = kt;

        const lg = blk.add(0x1800 + i * 0x20), lgbuf = blk.add(0x1a00 + i * 0x20);
        lg.add(4).writePointer(i % 3 ? lgbuf : ptr(0));
        out['lg' + i] = lg;

        out['jp' + i] = blk.add(0x4000 + i * 0x40);

        // bufferFree: four combinations of (record buffer null or not) x (this+0x18 zero or not)
        const bt = blk.add(0x3000 + i * 0x20), br = blk.add(0x3200 + i * 0x20);
        bt.add(0x18).writeU32((i & 2) ? (0x900 + i) : 0);
        br.add(4).writePointer((i & 1) ? blk.add(0x3400 + i * 0x20) : ptr(0));
        out['bt' + i] = bt;
        out['br' + i] = br;
      }

      // the singly linked lists listRemoveHead 0x004a4ed0 and listClear4a4d 0x004a4d50 walk
      const lens = [0, 1, 2, 3, 4, 1, 2, 0, 3, 1, 2, 4];
      let n = 0;
      for (let i = 0; i < 12; i++) {
        const ls = blk.add(0x1c00 + i * 0x20);
        ls.add(0xc).writeS32(lens[i]);
        ls.add(8).writeS32(0x4000 + i);
        out['ls' + i] = ls;
        if (lens[i] === 0) { ls.add(4).writePointer(ptr(0)); continue; }
        const nodes = [];
        for (let k = 0; k < lens[i]; k++) {
          const p = blk.add(0x1e00 + n * 0x20);
          // every other node carries a payload, so both sides of the +8 guard (0x004a4ee0,
          // 0x004a4d67) are taken inside one list
          p.add(8).writePointer((n % 2) ? blk.add(0x2600 + n * 0x20) : ptr(0));
          n += 1;
          nodes.push(p);
        }
        for (let k = 0; k < nodes.length; k++)
          nodes[k].add(0x10).writePointer(k + 1 < nodes.length ? nodes[k + 1] : ptr(0));
        ls.add(4).writePointer(nodes[0]);
      }

      // the NodeLists NodeList::clear 0x00487310 walks: +4 is the next node and +8 the continue flag
      let m = 0;
      for (let i = 0; i < 12; i++) {
        const nl = blk.add(0x3600 + i * 0x20);
        nl.add(0x18).writeS32(lens[i]);
        nl.add(0x10).writeS32(0x5000 + i);
        out['nl' + i] = nl;
        if (lens[i] === 0) { nl.add(0xc).writePointer(ptr(0)); continue; }
        const nodes = [];
        for (let k = 0; k < lens[i]; k++) { nodes.push(blk.add(0x3800 + m * 0x20)); m += 1; }
        for (let k = 0; k < nodes.length; k++) {
          const nxt = k + 1 < nodes.length ? nodes[k + 1] : ptr(0);
          nodes[k].add(4).writePointer(nxt);
          nodes[k].add(8).writePointer(nxt);
        }
        nl.add(0xc).writePointer(nodes[0]);
      }
      return out;
    },

    // ---------------------------------------------- sound.dll's linked-list helpers (c3cn.cpp part 4)
    // Stand-in over operator new 0x100424d7, malloc 0x10042acd and operator delete 0x1004249a.
    // A list is { head +0, tail +4, ?, count +0xc } and a node { prev +0, next +4, value +8 }.
    //   0x0000 twelve lists for the ten append helpers (stride 0x20): six already hold one node (the
    //          non-empty arm of the test at 0x10005e5e) and six are empty.
    //   0x0200 those twelve existing nodes, 0x0400 twelve values to append.
    //   0x0800 twelve lists for clearNodeList, popFront and their chains (stride 0x20), with 0 to 4
    //          nodes each from 0x0c00, linked through +4 with +8 as the continue flag.
    //   0x1800 twelve dynamic-array records for freeDynArray (+4 buffer, +8 flags), buffers at 0x1a00.
    c3cn_sound_heap() {
      const h = __heapStandIn({ module: 'sound.dll', alloc: [0x424d7, 0x42acd], free: [0x4249a],
                                arenaSize: 0x2000, fill: 0xcd });
      const blk = __keepAlloc(0x2000);
      fill(blk, 0x2000, 0x5d5d);
      const out = { arena: h.arena, freelog: h.log, sblk: blk, obj: h.arena };

      for (let i = 0; i < 12; i++) {
        const ls = blk.add(i * 0x20), nd = blk.add(0x200 + i * 0x20);
        if (i % 2) {
          nd.writePointer(ptr(0));
          nd.add(4).writePointer(ptr(0));
          nd.add(8).writeU32(0x3300 + i);
          ls.writePointer(nd);
          ls.add(4).writePointer(nd);
          ls.add(0xc).writeS32(1);
        } else {
          ls.writePointer(ptr(0));
          ls.add(4).writePointer(ptr(0));
          ls.add(0xc).writeS32(0);
        }
        out['pl' + i] = ls;
        out['pv' + i] = blk.add(0x400 + i * 0x20);

        // freeDynArray: six have a buffer at +4, and the flag word at +8 has bit 0 set in half of them
        const da = blk.add(0x1800 + i * 0x20);
        da.add(4).writePointer((i % 2) ? blk.add(0x1a00 + i * 0x20) : ptr(0));
        da.add(8).writeU32(((i & 2) ? 0x5551 : 0x5550) + i * 0x10);
        out['da' + i] = da;
      }

      const lens = [0, 1, 2, 3, 4, 1, 2, 0, 3, 1, 2, 4];
      let n = 0;
      for (let i = 0; i < 12; i++) {
        const ls = blk.add(0x800 + i * 0x20);
        ls.add(0xc).writeS32(lens[i]);
        out['cl' + i] = ls;
        if (lens[i] === 0) { ls.writePointer(ptr(0)); ls.add(4).writePointer(ptr(0)); continue; }
        const nodes = [];
        for (let k = 0; k < lens[i]; k++) { nodes.push(blk.add(0xc00 + n * 0x20)); n += 1; }
        for (let k = 0; k < nodes.length; k++) {
          const nxt = k + 1 < nodes.length ? nodes[k + 1] : ptr(0);
          nodes[k].add(4).writePointer(nxt);
          nodes[k].add(8).writePointer(nxt);          // popFront returns this value
        }
        ls.writePointer(nodes[0]);
        ls.add(4).writePointer(nodes[nodes.length - 1]);
      }
      return out;
    },

    // ------------------------ Terrain.dll's std::list<Tile*> allocation layer (c3cn.cpp part 5)
    // Stand-in over operator new 0x10018c90 and operator delete 0x10018be0. Terrain.dll is in the
    // exe's import table, so its hooks are installed before the window appears.
    // Nothing of the live Terrain instance is touched (getInstance 0x100031a0 is never called): the
    // lists, the allocator objects and the nodes are all in this block.
    //   0x0000 twelve std::list<Tile*> objects (allocator byte +0, sentinel node +4, size +8), stride
    //          0x20; 0x0200 twelve one-byte allocator objects to construct them from.
    //   0x0400 twelve scratch nodes handed to _Freenode and _Buynode, stride 0x20.
    c3cn_terrain_heap() {
      const h = __heapStandIn({ module: 'Terrain.dll', alloc: [0x18c90], free: [0x18be0],
                                arenaSize: 0x1000, fill: 0xcd });
      const blk = __keepAlloc(0x1000);
      fill(blk, 0x1000, 0x7e7e);
      const out = { arena: h.arena, freelog: h.log, tblk: blk, obj: h.arena };
      for (let i = 0; i < 12; i++) {
        out['tl' + i] = blk.add(i * 0x20);                       // list
        const al = blk.add(0x200 + i * 0x20);
        al.writeU8((0x41 + i) & 0xff);                           // the byte list::list copies to +0
        out['ta' + i] = al;
        out['tn' + i] = blk.add(0x400 + i * 0x20);               // scratch node
      }
      return out;
    },
  });

  function c3cnHeapBlock(arenaSize) {
    const h = standInAll(arenaSize);
    const blk = __keepAlloc(0x6000);
    fill(blk, 0x6000, 0x7a7a);
    const out = { arena: h.arena, freelog: h.log, blk: blk, obj: h.arena, hblk: blk };
    out.png = blk;

    for (let i = 0; i < 12; i++) out['p' + i] = blk.add(0x800 + i * 0x40);

    // Array<T> headers: vtable +0, buffer +4, capacity +8, count +0xc, growth step +0x10. Six have a
    // buffer (the operator-delete side of the guard at 0x10068020) and six have none.
    for (let i = 0; i < 12; i++) {
      const o = blk.add(0x1000 + i * 0x40), buf = blk.add(0x1800 + i * 0x100);
      o.add(4).writePointer(i % 2 ? buf : ptr(0));
      o.add(8).writeS32(4 + i);
      o.add(0xc).writeS32(i);
      o.add(0x10).writeS32(3 + i);
      out['ar' + i] = o;
    }

    // slot 2 of the sprite vtable: eax = ++*ctr; [ecx+0x38] = eax; ret   (thiscall, no argument)
    const ctr = blk.add(0x5f00);
    ctr.writeU32(0);
    const page = Process.pageSize;
    const code = __keepAlloc(page);
    Memory.protect(code, page, 'rwx');
    code.writeByteArray([0xA1, 0, 0, 0, 0, 0x40, 0xA3, 0, 0, 0, 0, 0x89, 0x41, 0x38, 0xC3]);
    code.add(1).writePointer(ctr);
    code.add(7).writePointer(ctr);
    const spriteVt = __keepAlloc(0x20);
    for (let i = 0; i < 8; i++) spriteVt.add(i * 4).writePointer(code);

    // [payload bytes, row count, has a row table] per source sprite: a zero payload makes jalloc(0)
    // carve nothing, and six sources have no row table (the `je 0x1001a1ea` at 0x1001a1b0).
    const sp = [[0, 0, 0], [1, 1, 1], [0x10, 0, 0], [0x20, 4, 1], [0x40, 2, 0], [0x80, 8, 1],
                [0x100, 16, 1], [0xc, 3, 0], [0x33, 5, 1], [4, 1, 0], [0x7f, 7, 1], [0x200, 0, 0]];
    for (let i = 0; i < 12; i++) {
      const [bytes, rows, hasrows] = sp[i];
      const dst = blk.add(0x2800 + i * 0x60), src = blk.add(0x2e00 + i * 0x60);
      dst.writePointer(spriteVt);
      src.writePointer(spriteVt);
      const pay = blk.add(0x3400 + i * 0x200), tab = blk.add(0x5800 + i * 0x40);
      for (let k = 0; k < bytes; k++) pay.add(k).writeU8((0x50 + i * 7 + k) & 0xff);
      for (let k = 0; k < rows; k++) tab.add(k * 4).writeU32((0x1000 * (i + 1) + k) >>> 0);
      src.add(0x10).writeS32(0x1100 + i);
      src.add(0x14).writePointer(pay);
      src.add(0x18).writeS32(0x2200 + i);
      src.add(0x1c).writePointer(hasrows ? tab : ptr(0));
      src.add(0x20).writeS32(0x3300 + i);
      src.add(0x24).writeS32(bytes);
      src.add(0x28).writeS32(0x4400 + i);
      src.add(0x2c).writeS32(0x5500 + i);
      src.add(0x30).writeS32(0x6600 + i);
      src.add(0x34).writeS32(rows);
      out['sd' + i] = dst;
      out['ss' + i] = src;
    }
    return out;
  }
})();
