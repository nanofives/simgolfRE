// Batch c3bw fixtures (jgld.dll): Quat::ctor 0x10003050, Quat::setIdentity 0x10003170, Matrix::ctor
// 0x10004590, Matrix::dtor 0x100032f0, Transform::ctor 0x10005450, Transform::dtor 0x100034e0,
// LinkedList::ctor 0x10006c80, LinkedList::dtor 0x10006cf0, LinkedList::clear 0x10006df0,
// LinkedList::removeCurrent 0x10007100 and inflate_trees_free 0x1009f250.
//
// Every block is allocated here; nothing live in the running jgld.dll (the Display 0x10128420, the Lib
// object 0x1012872c, the tracked-object list 0x10128428, a Surface, a Font, a Palette or the game's heap
// blocks) is handed to the A/B. Layouts come from the bodies' own field offsets, cited in
// shim/src/re/c3bw.cpp. Blocks are pre-filled with a per-offset word pattern and every record sits at a
// larger stride than the bytes it uses, so a store one field too wide or a field left unwritten changes
// the block hash.
//
// Two bodies destroy things through a pointer the CALLER owns, and that is what makes them A/B-able:
//   * LinkedList::clear and LinkedList::removeCurrent reach `delete node` through slot 0 of the node's
//     own vtable (0x10006e70, 0x10006ea8, 0x1000715d, 0x10007224). Every node here carries a vtable of
//     ours whose slot 0 is a stub that bumps a counter and writes it into node+0x14, so the deletions
//     are recorded in the compared state and nothing is handed to operator delete (which would make the
//     two arms free the same block twice).
//   * inflate_trees_free frees through the z_stream's zfree at +0x24 with the opaque at +0x28; the
//     z_stream here carries the same kind of counting stub.
Object.assign(globalThis.DIFF_FIXTURES, {

  // ------------------------- Quat, Matrix and Transform: six writers over their own objects.
  // Quat is 16 bytes (three floats plus the scalar at +0xc) at a 0x20 stride; Matrix is 0x44 (the vtable
  // pointer plus sixteen cells from +4) at a 0x60 stride; Transform is 0x24 (vtable, the quaternion at
  // +4, the position Vector3 at +0x14 and the flags word at +0x20) at a 0x40 stride.
  c3bw_math() {
    const out = {};
    const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

    // Twelve quaternions for the constructor and twelve for setIdentity, one block.
    const quat = Memory.alloc(24 * 0x20);
    fill(quat, 24 * 0x20, 0x7101);
    out.quatBlk = quat;
    for (let i = 0; i < 24; i++) {
      const p = quat.add(i * 0x20);
      // the four dwords the body rewrites, each starting from a different value
      p.writeU32((0x40000000 + i * 0x1111) >>> 0);
      p.add(4).writeU32((0xbf800000 + i) >>> 0);
      p.add(8).writeU32((0x7f7fffff - i * 0x10000) >>> 0);
      p.add(0xc).writeU32((0x12340000 + i) >>> 0);
      out[(i < 12 ? 'qc' : 'qi') + (i % 12)] = p;
    }

    // Twelve matrices for the constructor and twelve for the destructor.
    const mat = Memory.alloc(24 * 0x60);
    fill(mat, 24 * 0x60, 0x7202);
    out.matBlk = mat;
    for (let i = 0; i < 24; i++) {
      const p = mat.add(i * 0x60);
      p.writeU32((0x76760000 + i) >>> 0);                 // the vtable slot, overwritten with 0x1011d024
      for (let j = 0; j < 16; j++) p.add(4 + j * 4).writeU32((0x30000000 + i * 0x100 + j) >>> 0);
      out[(i < 12 ? 'mc' : 'md') + (i % 12)] = p;
    }

    // Twelve transforms for the constructor and twelve for the destructor.
    const xf = Memory.alloc(24 * 0x40);
    fill(xf, 24 * 0x40, 0x7303);
    out.xfBlk = xf;
    for (let i = 0; i < 24; i++) {
      const p = xf.add(i * 0x40);
      p.writeU32((0x77770000 + i) >>> 0);                 // vtable slot, overwritten with 0x1011d028
      for (let j = 0; j < 4; j++) p.add(4 + j * 4).writeU32((0x41000000 + i * 0x100 + j) >>> 0);   // quat
      for (let j = 0; j < 3; j++) p.add(0x14 + j * 4).writeU32((0x42000000 + i * 0x100 + j) >>> 0); // position
      p.add(0x20).writeU32((0x1e000000 + i) >>> 0);       // flags word, cleared by Transform::reset
      out[(i < 12 ? 'xc' : 'xd') + (i % 12)] = p;
    }
    return out;
  },

  // ----------------------------------------------- the LinkedList family: lists, nodes and the marker.
  // LinkedList: vptr +0, m_data +4, m_flag +8 (one byte), m_count +0xc, m_head +0x10, m_cur +0x14,
  // m_ordered +0x18; records at a 0x40 stride. ListNode: vptr +0, prev +4, next +8, data +0xc, flag +0x10
  // (one byte); records at a 0x20 stride, with the deleting destructor's marker landing at +0x14, just
  // past the node and inside the stride.
  //
  // The lists are built by replaying LinkedList::add 0x10006f40 here (the matched body in
  // re/match/jgld_list.cpp): the first node points at itself both ways, each later node is spliced in
  // before m_head, m_count is bumped and m_cur, m_data and m_flag follow the new node. No allocation of
  // the game's happens: the nodes are ours.
  c3bw_list() {
    const out = {};
    const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

    // the counter the deleting-destructor stub bumps; a state region of its own, so both arms start from
    // the same value and the recorded deletion order is comparable
    const ctr = Memory.alloc(4);
    ctr.writeU32(0);
    out.llCtr = ctr;

    // __thiscall stub(int flag): eax = ++*ctr; node[0x14] = eax; return eax; ret 4. The two callers store
    // the returned eax in a dead local ([ebp-0x24] at 0x10006eb1, [ebp-0x20] at 0x1000722d).
    //   A1 imm32            mov eax, [ctr]
    //   40                  inc eax
    //   A3 imm32            mov [ctr], eax
    //   89 41 14            mov [ecx+0x14], eax
    //   C2 04 00            ret 4   (the `push 1` of the caller is popped here)
    const code = Memory.alloc(Process.pageSize);
    Memory.protect(code, Process.pageSize, 'rwx');
    code.writeByteArray([0xA1, 0, 0, 0, 0, 0x40, 0xA3, 0, 0, 0, 0, 0x89, 0x41, 0x14, 0xC2, 0x04, 0x00]);
    code.add(1).writePointer(ctr);
    code.add(7).writePointer(ctr);
    out.llStub = code;

    // the node vtable: slot 0 is the scalar deleting destructor the two bodies call with the argument 1
    const vt = Memory.alloc(0x20);
    for (let i = 0; i < 8; i++) vt.add(i * 4).writePointer(code);
    out.llVtbl = vt;

    const NL = 48, NN = 160;
    const llBlk = Memory.alloc(NL * 0x40);
    fill(llBlk, NL * 0x40, 0x7404);
    out.llBlk = llBlk;
    const nodeBlk = Memory.alloc(NN * 0x20);
    fill(nodeBlk, NN * 0x20, 0x7505);
    out.nodeBlk = nodeBlk;

    let nextNode = 0;
    const newNode = () => {
      const p = nodeBlk.add(nextNode * 0x20);
      p.writePointer(vt);                                   // vptr
      p.add(4).writePointer(ptr(0));                        // prev
      p.add(8).writePointer(ptr(0));                        // next
      p.add(0xc).writeU32((0x51000000 + nextNode) >>> 0);   // data: a constant, never dereferenced
      p.add(0x10).writeU8((nextNode * 7 + 3) & 0xff);       // flag
      p.add(0x14).writeU32(0);                              // the marker slot
      nextNode++;
      return p;
    };

    // LinkedList::add 0x10006f40 without the allocation (the node is given).
    const listAdd = (L, n) => {
      L.add(0x18).writeU32(0);                              // m_ordered = 0
      const head = L.add(0x10).readPointer();
      if (!head.isNull()) {
        const hp = head.add(4).readPointer();               // m_head->prev
        n.add(4).writePointer(hp);
        n.add(8).writePointer(head);
        head.add(4).writePointer(n);
        hp.add(8).writePointer(n);
      } else {
        L.add(0x10).writePointer(n);
        n.add(4).writePointer(n);
        n.add(8).writePointer(n);
      }
      L.add(0xc).writeU32(L.add(0xc).readU32() + 1);        // m_count++
      L.add(0x14).writePointer(n);                          // m_cur = the new node
      L.add(4).writeU32(n.add(0xc).readU32());              // m_data
      L.add(8).writeU8(n.add(0x10).readU8());               // m_flag
    };

    let nextList = 0;
    // size = how many nodes to add; cur = 'tail' (as add left it), 'head' or an index into the chain
    const newList = (size, cur) => {
      const L = llBlk.add(nextList * 0x40);
      nextList++;
      L.writeU32((0x78780000 + nextList) >>> 0);            // vtable slot (LinkedList::dtor rewrites it)
      L.add(4).writeU32((0x79790000 + nextList) >>> 0);     // m_data
      L.add(8).writeU8(0xa5);                               // m_flag
      L.add(0xc).writeU32(0);                               // m_count
      L.add(0x10).writePointer(ptr(0));                     // m_head
      L.add(0x14).writePointer(ptr(0));                     // m_cur
      L.add(0x18).writeU32((0x7b7b0000 + nextList) >>> 0);  // m_ordered: non-zero, cleared by clear()
      const nodes = [];
      for (let i = 0; i < size; i++) { const n = newNode(); nodes.push(n); listAdd(L, n); }
      if (size > 0 && cur === 'head') L.add(0x14).writePointer(nodes[0]);
      if (size > 0 && typeof cur === 'number') L.add(0x14).writePointer(nodes[cur]);
      return L;
    };

    // LinkedList::ctor 0x10006c80: twelve untouched list objects (it reads nothing).
    for (let i = 0; i < 12; i++) out['lc' + i] = newList(0, 'tail');

    // LinkedList::dtor 0x10006cf0: ten lists, sizes 0, 1, 2, 3, 4, 5, 6, 8, 1, 0.
    [0, 1, 2, 3, 4, 5, 6, 8, 1, 0].forEach((s, i) => { out['ld' + i] = newList(s, 'tail'); });

    // LinkedList::clear 0x10006df0: twelve lists, sizes 0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 0. The loop at
    // 0x10006e41 runs size - 2 times for size >= 2 and zero times for size 0 and 1.
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 0].forEach((s, i) => { out['lk' + i] = newList(s, 'tail'); });

    // LinkedList::removeCurrent 0x10007100: fourteen lists. 'head' puts m_cur back on m_head (the arm at
    // 0x100071a1), a number puts it on that node of the chain, 'tail' leaves it where add() left it.
    [[0, 'tail'], [1, 'tail'], [2, 'tail'], [2, 'head'], [3, 'tail'], [3, 'head'], [3, 1],
     [4, 'tail'], [4, 'head'], [5, 'tail'], [5, 'head'], [6, 2], [1, 'head'], [2, 'head']
    ].forEach((v, i) => { out['lr' + i] = newList(v[0], v[1]); });

    return out;
  },

  // ---------------------------------------------------------- inflate_trees_free 0x1009f250 (zlib 1.0.2).
  // huft entries are eight bytes and huft_build links each table through a dummy entry just below it, so
  // the table pointer t the function is given has its chain link at t-4 and the block's own base at t-8.
  // Each chain here is built in its own 0x200 slice of one block, eight 0x40 blocks at most; block j's
  // link points at block j+1's table and the last one's is null.
  c3bw_zlib() {
    const out = {};
    const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

    const ctr = Memory.alloc(4);
    ctr.writeU32(0);
    out.zCtr = ctr;

    // __cdecl stub(opaque, p): eax = ++*ctr; *p = eax; ret. The block it is given is the one the second
    // loop stepped back to, so the mark lands on the dummy entry's first dword and the recorded order of
    // the frees is part of the compared state.
    //   A1 imm32 / 40 / A3 imm32 / 8B 4C 24 08 / 89 01 / C3
    const code = Memory.alloc(Process.pageSize);
    Memory.protect(code, Process.pageSize, 'rwx');
    code.writeByteArray([0xA1, 0, 0, 0, 0, 0x40, 0xA3, 0, 0, 0, 0, 0x8B, 0x4C, 0x24, 0x08, 0x89, 0x01, 0xC3]);
    code.add(1).writePointer(ctr);
    code.add(7).writePointer(ctr);
    out.zStub = code;

    // a z_stream of ours: only zfree at +0x24 and opaque at +0x28 are read (0x1009f2bc, 0x1009f2c3).
    const z = Memory.alloc(0x40);
    fill(z, 0x40, 0x7606);
    z.add(0x24).writePointer(code);
    z.add(0x28).writeU32(0x5a5a5a5a);        // opaque: handed to the stub, never dereferenced
    out.zStream = z;

    const K = [0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 3, 2];
    const STRIDE = 0x200, BLK = 0x40;
    const blk = Memory.alloc(K.length * STRIDE);
    fill(blk, K.length * STRIDE, 0x7707);
    out.zBlk = blk;
    K.forEach((k, i) => {
      const base = blk.add(i * STRIDE);
      for (let j = 0; j < k; j++) {
        const b = base.add(j * BLK);
        b.writeU32((0x6c6c0000 + i * 0x100 + j) >>> 0);     // the dummy entry's first dword: the stub's mark
        b.add(4).writePointer(j + 1 < k ? base.add((j + 1) * BLK + 8) : ptr(0));   // the chain link at t-4
        for (let e = 8; e < BLK; e += 4)
          b.add(e).writeU32((0x6d6d0000 + i * 0x100 + j * 0x10 + e) >>> 0);        // the table entries
      }
      if (k > 0) out['zT' + i] = base.add(8);               // t = the first block's table
    });
    return out;
  },
});
