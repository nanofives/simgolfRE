// Fixtures for C3 batch c3dm (round 26, golf_clean.exe). Only our own synthetic values (no game text, no live game
// object except the globals named below, which freeze.js saves and restores). Names are prefixed c3dm_.
//
// Part 1 (c3dm_dtors, c3dm_dd401): scalar / deleting destructors and list helpers under the heap stand-in over free
// 0x004a5007 / malloc 0x004a504f / operator new 0x004a57f1 (operator delete 0x004a4ffc tail-calls free), so every
// release the arms perform is a recorded pointer in $freelog, compared state. The first thread is frozen (freeze.js) for
// these keys, so no game thread runs through the patched allocator entries. Objects live in one 0x8000-byte block
// ($eblk, pre-filled 0x6d6d6d6d so every store a destructor makes is a visible change).
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function fill(p, n, v) { for (let i = 0; i < n; i += 4) p.add(i).writeU32(v >>> 0); }
  function zero(p, n) { fill(p, n, 0); }
  function alloc(n, v) { const p = Memory.alloc(n); if (v === undefined) zero(p, n); else fill(p, n, v); return p; }
  function mkRec() { return alloc(0x40); }
  function push(rec, v) {
    let h = rec.readU32();
    h = (Math.imul(h, 33) + (v >>> 0)) >>> 0;
    rec.writeU32(h);
    rec.add(4).writeU32((rec.add(4).readU32() + 1) >>> 0);
    rec.add(8).writeU32(v >>> 0);
  }
  const BLK = 0x8000;
  const U = (v) => (v >>> 0);

  // Allocates the heap stand-in and the object block shared by Part 1.
  function heapBlock() {
    __freeze.thread();                // the game's first thread is stopped before the allocator entries are patched
    const h = __heapStandIn({ module: 'golf_clean.exe', alloc: [0xa504f, 0xa57f1], free: [0xa5007],
                              arenaSize: 0x2000, fill: 0xcd });
    const blk = alloc(BLK, 0x6d6d6d6d);
    const rec = mkRec();
    return { h: h, blk: blk, rec: rec, out: { arena: h.arena, freelog: h.log, eblk: blk, obj: blk, rec: rec } };
  }

  // A doubly linked chain of nodes {prev +0, next +4, continue flag +8}; node k is taken from the pool.
  function Pool(blk, start) {
    let n = 0;
    return {
      node(prevP, nextP, cont) {
        const p = blk.add(start + n * 0x20); n += 1;
        p.writePointer(prevP); p.add(4).writePointer(nextP); p.add(8).writeU32(U(cont));
        return p;
      },
      // len nodes; contAt(k) gives node k's +8; returns the node array (linked prev/next)
      chain(len, contAt) {
        const nodes = [];
        for (let k = 0; k < len; k++) nodes.push(this.node(ptr(0), ptr(0), contAt(k)));
        for (let k = 0; k < len; k++) {
          nodes[k].writePointer(k > 0 ? nodes[k - 1] : ptr(0));
          nodes[k].add(4).writePointer(k + 1 < len ? nodes[k + 1] : ptr(0));
        }
        return nodes;
      },
      used() { return n; },
    };
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ---------------------------------------------------------------------------------------------------------
    // Objects of every Part 1 destructor, twelve of each kind (block offsets):
    //   0x0000 Bufs (stride 0x40): the buffer pointer at +8 is set in the odd ones            -> $bf0..$bf11
    //   0x0400 StrLists (0x20): a child with a vtable whose slot +0x14 is our recorder, at +0x14 in nine of them
    //   0x0600 KeyTables (0x140): the embedded Buf at +0x108, its buffer at +0x110 in the odd ones
    //   0x1500 loggers (0x20): the buffer at +4 in two of three
    //   0x1700 List480 headers (0x20) with records from 0x1900 (0x10 each; payload +8, next +0xc)
    //   0x1d00 clearScalarDtor lists (0x20): head +4, tail +8, count +0x10
    //   0x2700 hash tables for NodeList::dtor (0x200): 16 buckets of 0x1c from +0x24, child at +0x14
    //   nodes from 0x3f00 (0x20 each); recorded pointer targets (never dereferenced) from 0x7000
    c3dm_dtors() {
      const hb = heapBlock();
      const blk = hb.blk, rec = hb.rec, out = hb.out;
      const DATA = 0x7000;
      const pool = Pool(blk, 0x3f00);

      // the child objects StrList::dtor dispatches to through [[child] + 0x14]
      const vt = alloc(0x40, 0);
      vt.add(0x14).writePointer(keep(new NativeCallback(function (self) {
        push(rec, self.add(4).readS32());
      }, 'void', ['pointer'], 'thiscall')));
      const child = function (id) { const k = alloc(0x20, 0); k.writePointer(vt); k.add(4).writeS32(id); return k; };
      const kids = [null, child(1), child(2), child(-7), child(0x1234), child(0), null, child(9), child(3)];

      for (let i = 0; i < 12; i++) {
        const bf = blk.add(i * 0x40);
        bf.add(8).writePointer(i % 2 ? blk.add(DATA + i * 0x10) : ptr(0));
        out['bf' + i] = bf;

        const sl = blk.add(0x400 + i * 0x20);
        const k = kids[i % kids.length];
        sl.add(0x14).writePointer(k === null ? ptr(0) : k);
        out['sl' + i] = sl;

        const kt = blk.add(0x600 + i * 0x140);
        kt.add(0x108 + 8).writePointer(i % 2 ? blk.add(DATA + 0x100 + i * 0x10) : ptr(0));
        out['kt' + i] = kt;

        const lg = blk.add(0x1500 + i * 0x20);
        lg.add(4).writePointer(i % 3 ? blk.add(DATA + 0x200 + i * 0x10) : ptr(0));
        out['lg' + i] = lg;
      }

      // List480 / BinkFrame::scalarDtor lists: [records in the chain, count, flag at +0x14, payloads present]
      const LL = [[0, 0, 0, 1], [0, 5, 0, 1], [1, 1, 0, 1], [2, 2, 0, 3], [3, 3, 0, 7], [4, 4, 0, 0], [2, 2, 1, 3],
                  [1, 0, 0, 1], [2, -3, 0, 3], [3, 2, 0, 7], [1, 1, 0, 1], [5, 5, 0, 31]];
      let rn = 0;
      LL.forEach(function (r, i) {
        const ll = blk.add(0x1700 + i * 0x20);
        const recs = [];
        for (let k = 0; k < r[0]; k++) recs.push(blk.add(0x1900 + (rn++) * 0x10));
        recs.forEach(function (p, k) {
          p.add(8).writePointer((r[3] >> k) & 1 ? blk.add(DATA + 0x300 + (rn + k) * 0x10) : ptr(0));
          p.add(0xc).writePointer(k + 1 < recs.length ? recs[k + 1] : ptr(0));
        });
        ll.add(4).writePointer(recs.length ? recs[0] : ptr(0));
        ll.add(0xc).writeS32(r[1]);
        ll.add(0x10).writeU32(0x77 + i);
        ll.add(0x14).writeU32(r[2] || (i === 6 ? 1 : 0));
        out['ll' + i] = ll;
      });

      // clearScalarDtor lists: node counts and where a node's continue flag is 0 (the loop stops after it)
      const LEN = [0, 1, 2, 3, 4, 1, 2, 0, 3, 1, 4, 4];
      const STOP = { 3: 0, 5: 0, 8: 1, 9: 0, 10: 1, 11: 3 };   // list -> the node index whose +8 is 0
      LEN.forEach(function (len, i) {
        const nl = blk.add(0x1d00 + i * 0x20);
        const nodes = pool.chain(len, function (k) { return STOP[i] === k ? 0 : 0x500 + k; });
        nl.add(4).writePointer(nodes.length ? nodes[0] : ptr(0));
        nl.add(8).writePointer(nodes.length ? nodes[nodes.length - 1] : ptr(0));
        nl.add(0x10).writeS32(len);
        out['nl' + i] = nl;
      });

      // singly linked lists of listClear4a4d 0x004a4d50 / listClearThunk: head +4, next +0x10, payload +8 (null in
      // every other node); lists of 0..4 nodes, list +8 and +0xc pre-filled with the pattern, cleared by the call
      const LCN = [0, 1, 2, 3, 4, 1, 2, 0, 3, 1, 2, 4];
      let pn = 0;
      LCN.forEach(function (len, i) {
        const lc = blk.add(0x2000 + i * 0x20);
        const nodes = [];
        for (let k = 0; k < len; k++, pn++)
          nodes.push(pool.node(ptr(0), ptr(0), (pn % 2) ? 0 : 0x9000 + pn * 0x10));
        nodes.forEach(function (nd, k) { nd.add(0x10).writePointer(k + 1 < len ? nodes[k + 1] : ptr(0)); });
        lc.add(4).writePointer(len ? nodes[0] : ptr(0));
        out['lc' + i] = lc;
      });

      // hash tables for NodeList::dtor: bucket b of table i holds (i * 3 + b * 5) % 4 nodes; a node's continue flag is
      // 0 for the last node of the chain, so the clear walks the whole chain
      for (let i = 0; i < 12; i++) {
        const ht = blk.add(0x2700 + i * 0x200);
        const kd = kids[(i + 2) % kids.length];
        ht.add(0x14).writePointer(kd === null ? ptr(0) : kd);
        for (let b = 0; b < 16; b++) {
          const el = ht.add(0x24 + 0x1c * b);
          const len = (i * 3 + b * 5) % 4;
          const nodes = pool.chain(len, function (k) { return k + 1 < len ? 0x600 + k : 0; });
          el.add(0xc).writePointer(nodes.length ? nodes[0] : ptr(0));
          el.add(0x10).writePointer(nodes.length ? nodes[nodes.length - 1] : ptr(0));
          el.add(0x18).writeS32(len);
        }
        out['ht' + i] = ht;
      }
      return out;
    },

    // ---------------------------------------------------------------------------------------------------------
    // HashTable::remove: one table of six used buckets, items 0x40 bytes with the bucket index at +0x3c.
    //   bucket 0: [A0]            bucket 1: [B0 B1 B2]      bucket 2: [C0 C1]       bucket 3: empty
    //   bucket 4: [D0 D1]         bucket 5: [E0 E1 E2 E3]   (X4 claims bucket 4 but is in no chain)
    c3dm_hashremove() {
      const hb = heapBlock();
      const blk = hb.blk, out = hb.out;
      const pool = Pool(blk, 0x3f00);
      const hr = blk.add(0x2000);
      out.hr = hr;
      zero(hr, 0x200);               // every bucket without a chain has a null head (buckets 3 and 6..15)
      const items = {};
      let ni = 0;
      const item = function (name, bucket) {
        const it = blk.add(0x2200 + (ni++) * 0x40);
        it.add(0x3c).writeU32(U(bucket));
        items[name] = it; out[name] = it;
        return it;
      };
      const layout = [['A', 1], ['B', 3], ['C', 2], ['D', 2], ['E', 4]];
      layout.forEach(function (row, b) {
        const its = [];
        for (let k = 0; k < row[1]; k++) its.push(item(row[0] + k, b));
        const nodes = pool.chain(its.length, function () { return 0; });
        nodes.forEach(function (nd, k) { nd.add(8).writePointer(its[k]); });
        const el = hr.add(0x24 + 0x1c * b), esi = el.add(8);
        esi.add(4).writePointer(nodes[0]);
        esi.add(8).writePointer(nodes[nodes.length - 1]);
        esi.add(0xc).writeU32(0x1111 + b);
        esi.add(0x10).writeS32(its.length);
      });
      item('X3', 3);            // bucket 3 has no head
      item('X4', 4);            // claims bucket 4, in no chain
      item('Z10', 0x10);
      item('ZFF', 0xffffffff);
      item('Z7', 7);            // an unused bucket
      return out;
    },

    // ---------------------------------------------------------------------------------------------------------
    // deletingDtor_401c30: objects of 0x10 bytes, +4 the dword the global 0x00839650 receives (0 in the even ones).
    // The global is written by many destructors of the game, so the first thread is frozen and the global restored.
    c3dm_dd401() {
      const hb = heapBlock();
      const out = hb.out, blk = hb.blk;
      for (let i = 0; i < 12; i++) {
        const o = blk.add(0x1f00 + i * 0x10);
        o.add(4).writeU32(i % 2 ? 0x8000 + i : 0);
        out['dd' + i] = o;
      }
      __freeze.save(ptr(0x00839650), 4);          // the thread was already frozen by heapBlock()
      return out;
    },
  });

  // ======================================================================================================
  // Part 2 - the game's global grids. Every key freezes the game's first thread (freeze.js) and saves what it writes,
  // so nothing the game reads or writes concurrently moves and the globals return to their values at the flush.
  //   0x00822d9c  the generator Random::range advances (one dword)
  //   0x00838aa0  genNoise's tables (0x2bd bytes used)
  //   0x005722e8  tile type bytes, 50 x 50 (index 50 x + y)           0x0056988c  path grid, 50 x 50 bytes
  //   0x0053caf0  coverage words, 50 x 50 x 2 bytes                  0x00578370  tile type table: stride 0x30, the byte
  //   at +2 (0x00578372) is floodMark's reach and the byte at +6 (0x00578376) is the kind markTileKind11 tests
  const SEEDS = [0x00000001, 0x12345678, 0x9e3779b9, 0xdeadbeef, 0x00000000, 0x7fffffff, 0x80000000, 0xffffffff,
                 0x0badf00d, 0xcafebabe, 0x2468ace0, 0x55aa55aa];
  const TYPES = 0x005722e8, GRID = 0x0056988c, WORDS = 0x0053caf0, TABLE = 0x00578370;

  function noiseFx(i) {
    return function () {
      __freeze.thread();
      __freeze.setU32(ptr(0x00822d9c), SEEDS[i]);
      __freeze.fill(ptr(0x00838aa0), 0x2c0, 0x6d);
      return {};
    };
  }

  // tile world: typeAt(x, y) -> type byte; table: { type: [reach, kind] }; cell(x,y) -> [word, grid byte]
  function tileWorld(typeAt, table, wordAt, gridAt) {
    __freeze.thread();
    __freeze.save(ptr(TYPES), 2500);
    __freeze.save(ptr(GRID), 2500);
    __freeze.save(ptr(WORDS), 5000);
    __freeze.save(ptr(TABLE), 0x30 * 24);
    const ty = [], gr = [];
    for (let x = 0; x < 50; x++) for (let y = 0; y < 50; y++) {
      ty.push(typeAt(x, y) & 0xff);
      gr.push(gridAt(x, y) & 0xff);
      ptr(WORDS).add((x * 50 + y) * 2).writeU16(wordAt(x, y));
    }
    ptr(TYPES).writeByteArray(ty);
    ptr(GRID).writeByteArray(gr);
    for (let t = 0; t < 24; t++) {
      const row = table[t] || [0, 0];
      ptr(TABLE).add(t * 0x30 + 2).writeS8(row[0]);
      ptr(TABLE).add(t * 0x30 + 6).writeU8(row[1]);
    }
    return {};
  }

  // type table: 0 plain (reach 0, kind 0), 1 kind 0x11 reach 3, 2 plain, 3 reach -5, 5 reach 1, 20 (0x14) kind 0x11
  const TABLE_ROWS = { 0: [0, 0], 1: [3, 0x11], 2: [0, 0], 3: [-5, 0], 4: [2, 0x11], 5: [1, 0], 20: [2, 0x11] };
  const PATH0 = (x, y) => (x * 3 + y * 5) % 4;
  const KIND_WORLDS = [
    // 0: isolated kind-11 tiles
    (x, y) => ((x === 10 && y === 10) || (x === 30 && y === 30) || (x === 44 && y === 5)) ? 1 : 2,
    // 1: a 5 x 5 block
    (x, y) => (x >= 20 && x <= 24 && y >= 20 && y <= 24) ? 1 : 2,
    // 2: an L shape
    (x, y) => ((x === 5 && y >= 5 && y <= 15) || (y === 15 && x >= 5 && x <= 15)) ? 1 : 2,
    // 3: a checkerboard in a 10 x 10 region
    (x, y) => (x >= 30 && x < 40 && y >= 30 && y < 40 && ((x + y) & 1) === 0) ? 1 : 2,
    // 4: no kind-11 tile at all
    (x, y) => 2,
    // 5: a scatter over the whole map (about 30 percent)
    (x, y) => (((x * 2654435761 + y * 40503 + 12345) >>> 7) % 10) < 3 ? 1 : 2,
    // 6: kind-11 tiles that are blocked (type 0x14, tileBlocked 0x0040bf60 reports them) mixed into an 8 x 8 block
    (x, y) => (x >= 12 && x < 20 && y >= 12 && y < 20) ? (((x * y) % 3) === 0 ? 20 : 4) : 0,
    // 7: the border ring of the map
    (x, y) => (x === 0 || x === 49 || y === 0 || y === 49) ? 1 : 2,
  ];

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dm_noise_0: noiseFx(0), c3dm_noise_1: noiseFx(1), c3dm_noise_2: noiseFx(2), c3dm_noise_3: noiseFx(3),
    c3dm_noise_4: noiseFx(4), c3dm_noise_5: noiseFx(5), c3dm_noise_6: noiseFx(6), c3dm_noise_7: noiseFx(7),
    c3dm_noise_8: noiseFx(8), c3dm_noise_9: noiseFx(9), c3dm_noise_10: noiseFx(10), c3dm_noise_11: noiseFx(11),

    c3dm_kind_0() { return tileWorld(KIND_WORLDS[0], TABLE_ROWS, () => 0, PATH0); },
    c3dm_kind_1() { return tileWorld(KIND_WORLDS[1], TABLE_ROWS, () => 0, PATH0); },
    c3dm_kind_2() { return tileWorld(KIND_WORLDS[2], TABLE_ROWS, () => 0, PATH0); },
    c3dm_kind_3() { return tileWorld(KIND_WORLDS[3], TABLE_ROWS, () => 0, PATH0); },
    c3dm_kind_4() { return tileWorld(KIND_WORLDS[4], TABLE_ROWS, () => 0, PATH0); },
    c3dm_kind_5() { return tileWorld(KIND_WORLDS[5], TABLE_ROWS, () => 0, PATH0); },
    c3dm_kind_6() { return tileWorld(KIND_WORLDS[6], TABLE_ROWS, () => 0, PATH0); },
    c3dm_kind_7() { return tileWorld(KIND_WORLDS[7], TABLE_ROWS, () => 0, PATH0); },

    // floodMark: a 12 x 12 region A (x, y in 8..19) of reach-3 tiles whose words carry 0x20, 0x400 or 0x420, with two
    // sink tiles (type 3, reach -5), a visited strip (bit 0x40 in (10, 10..12)) and junk in the unrelated bits; a
    // region B (x, y in 30..34) of reach-1 tiles with 0x400; an isolated reach-3 tile at (25, 25) with an empty word.
    c3dm_flood() {
      const inA = (x, y) => x >= 8 && x <= 19 && y >= 8 && y <= 19;
      const inB = (x, y) => x >= 30 && x <= 34 && y >= 30 && y <= 34;
      return tileWorld(
        (x, y) => inA(x, y) ? (((x === 12 && y === 12) || (x === 13 && y === 13)) ? 3 : 1)
                : inB(x, y) ? 5 : (x === 25 && y === 25) ? 1 : 2,
        TABLE_ROWS,
        (x, y) => {
          let w = 0;
          if (inA(x, y)) w = ((x + y) % 3 === 0) ? 0x400 : (((x * y) % 5 === 0) ? 0x420 : 0x20);
          if (inB(x, y)) w = 0x400;
          if (x === 10 && y >= 10 && y <= 12) w |= 0x40;
          if ((x * 7 + y * 11) % 13 === 0) w |= 0x8001;        // junk bits floodMark must leave alone
          return w;
        },
        () => 0);
    },
  });

  // ======================================================================================================
  // Part 3 - the libjpeg 6a compression API. Every key hands the functions synthetic compress structs (one 0x600-byte
  // block each, pre-filled 0x3c3c3c3c, laid out as below) whose callbacks are recorders. Nothing here touches a live
  // game object. A callback is a pure function of its arguments and of memory diff_hook snapshots and restores between
  // the arms; the shared recorder $jrec (hash + count + last value) makes the ORDER and the arguments of every dispatch
  // compared state.
  //   +0x000 jpeg_compress_struct (0x180)   +0x180 error manager (0x84)   +0x240 memory manager (0x40)
  //   +0x280 progress monitor (0x20)        +0x2a0 destination manager    +0x2c0 comp master (prepare_for_pass +0,
  //   pass_startup +4, finish_pass +8, call_pass_startup +0xc, is_last_pass +0x10, our pass counter +0x14 and limit +0x18)
  //   +0x2e0 main controller (process_data +4, our mode +8)   +0x300 coefficient controller (compress_data +4, our
  //   period +8, our call counter +0xc)   +0x320 marker writer (write_file_trailer +0x10)   +0x340.. spare
  const JB = 0x600;
  function jEnv() {
    const rec = mkRec();
    const e = { rec: rec };
    const mk = function (ret, args, fn) { return keep(new NativeCallback(fn, ret, args)); };
    e.errExit = mk('void', ['pointer'], function (c) {
      const er = c.readPointer();
      push(rec, 0x01000000 + (er.add(0x14).readU32() & 0xffff));
      push(rec, er.add(0x18).readU32());
    });
    e.emit = mk('void', ['pointer', 'int'], function (c, level) {
      const er = c.readPointer();
      push(rec, 0x02000000 + (er.add(0x14).readU32() & 0xffff));
      push(rec, level >>> 0);
    });
    e.reset = mk('void', ['pointer'], function (c) { push(rec, 0x03000000 + (c.add(0x10).readU32() & 0xffff)); });
    e.init = mk('void', ['pointer'], function (c) { push(rec, 0x04000000 + (c.add(0x10).readU32() & 0xffff)); });
    e.term = mk('void', ['pointer'], function (c) { push(rec, 0x05000000 + (c.add(0x10).readU32() & 0xffff)); });
    e.prog = mk('void', ['pointer'], function (c) {
      const p = c.add(8).readPointer();
      push(rec, 0x06000000 + (p.add(4).readU32() & 0xffff));
      push(rec, p.add(8).readU32());
    });
    e.prep = mk('void', ['pointer'], function (c) {
      const m = c.add(0x144).readPointer();
      const n = m.add(0x14).readU32();
      m.add(0x14).writeU32(n + 1);
      push(rec, 0x07000000 + n);
    });
    e.startup = mk('void', ['pointer'], function (c) { push(rec, 0x08000000 + (c.add(0xd8).readU32() & 0xffff)); });
    e.finish = mk('void', ['pointer'], function (c) {
      const m = c.add(0x144).readPointer();
      const n = m.add(0x14).readU32();
      push(rec, 0x09000000 + n);
      if (n >= m.add(0x18).readU32()) m.add(0x10).writeU32(1);      // the last pass is the one that reaches the limit
    });
    e.proc = mk('void', ['pointer', 'pointer', 'pointer', 'uint32'], function (c, rows, ctr, n) {
      const mode = c.add(0x148).readPointer().add(8).readU32();
      const r = mode === 0 ? n : mode === 1 ? (n >>> 1) : mode === 2 ? 0 : (n > 0 ? (n - 1) >>> 0 : 0);
      push(rec, 0x0a000000 + (n & 0xffffff));
      push(rec, r);
      push(rec, rows.toInt32() >>> 0);
      ctr.writeU32(r);
    });
    e.coef = mk('int', ['pointer', 'pointer'], function (c, buf) {
      const k = c.add(0x150).readPointer();
      const period = k.add(8).readU32(), cnt = k.add(0xc).readU32();
      k.add(0xc).writeU32(cnt + 1);
      push(rec, 0x0b000000 + cnt);
      return (period === 0 || ((cnt + 1) % period) !== 0) ? 1 : 0;
    });
    e.trailer = mk('void', ['pointer'], function (c) { push(rec, 0x0c000000 + (c.add(0x10).readU32() & 0xffff)); });
    e.freePool = mk('void', ['pointer', 'int'], function (c, pool) { push(rec, 0x0d000000 + pool); });
    e.jinit = mk('void', ['pointer'], function (c) {
      push(rec, 0x0e000000 + (c.add(0x10).readU32() & 0xffff));
      push(rec, c.add(0x1c).readU32());
    });
    // alloc_small(cinfo, pool_id, size): a bump allocator over the pool at mem+0x30 (cursor +0, request hash +4)
    e.alloc = mk('pointer', ['pointer', 'int', 'uint32'], function (c, pool, size) {
      const p = c.add(4).readPointer().add(0x30).readPointer();
      const cur = p.readU32();
      p.writeU32((cur + ((size + 15) & ~15)) >>> 0);
      p.add(4).writeU32((Math.imul(p.add(4).readU32(), 33) + pool * 0x10000 + size) >>> 0);
      return p.add(0x10 + cur);
    });
    return e;
  }

  // One compress struct. o: state, cs (in_color_space), h (image_height), next, raw, prog (progress monitor present),
  // startup (master call_pass_startup), last (is_last_pass), limit (passes before our finish_pass reports the last),
  // rows (total_iMCU_rows), mode (main controller mode), period (coef controller suspend period), comp (comp_info
  // present), tables (quant / huffman table pointers present), pool (alloc_small pool size, 0 for none).
  function jMake(e, o, fx, i) {
    const b = alloc(JB, 0x3c3c3c3c);
    zero(b, 0x180);
    zero(b.add(0x180), 0x84);
    zero(b.add(0x240), 0x40);
    zero(b.add(0x280), 0x20); zero(b.add(0x2a0), 0x20); zero(b.add(0x2c0), 0x20); zero(b.add(0x2e0), 0x20);
    zero(b.add(0x300), 0x20); zero(b.add(0x320), 0x20);
    const er = b.add(0x180), mem = b.add(0x240), prog = b.add(0x280), dest = b.add(0x2a0), master = b.add(0x2c0),
          main = b.add(0x2e0), coef = b.add(0x300), marker = b.add(0x320);
    er.writePointer(e.errExit); er.add(4).writePointer(e.emit); er.add(0x10).writePointer(e.reset);
    mem.writePointer(e.alloc); mem.add(0x24).writePointer(e.freePool);
    if (o.pool) {
      const pool = alloc(0x1100, 0xcccccccc);
      zero(pool, 0x10);
      mem.add(0x30).writePointer(pool);
      fx['p' + i] = pool;
    }
    prog.writePointer(e.prog);
    dest.add(8).writePointer(e.init); dest.add(0x10).writePointer(e.term);
    master.writePointer(e.prep); master.add(4).writePointer(e.startup); master.add(8).writePointer(e.finish);
    master.add(0xc).writeU32(o.startup || 0); master.add(0x10).writeU32(o.last || 0);
    master.add(0x14).writeU32(0); master.add(0x18).writeU32(o.limit || 0);
    main.add(4).writePointer(e.proc); main.add(8).writeU32(o.mode || 0);
    coef.add(4).writePointer(e.coef); coef.add(8).writeU32(o.period || 0); coef.add(0xc).writeU32(0);
    marker.add(0x10).writePointer(e.trailer);
    b.writePointer(er); b.add(4).writePointer(mem);
    b.add(8).writePointer(o.prog ? prog : ptr(0));
    b.add(0x10).writeU32(o.state >>> 0);
    b.add(0x14).writePointer(dest);
    b.add(0x1c).writeU32((o.h === undefined ? 10 : o.h) >>> 0);
    b.add(0x24).writeU32((o.cs === undefined ? 2 : o.cs) >>> 0);
    b.add(0xa8).writeU32((o.raw || 0) >>> 0);
    b.add(0xd8).writeU32((o.next || 0) >>> 0);
    b.add(0xe8).writeU32((o.rows || 0) >>> 0);
    b.add(0x144).writePointer(master); b.add(0x148).writePointer(main); b.add(0x150).writePointer(coef);
    b.add(0x154).writePointer(marker);
    if (o.comp) {
      const k = alloc(10 * 0x54, 0x99999999);
      b.add(0x3c).writePointer(k);
      fx['k' + i] = k;
    }
    if (o.tables) {
      const t = alloc(0x1000, 0x7a7a7a7a);
      for (let q = 0; q < 4; q++) {
        b.add(0x40 + q * 4).writePointer(q < 2 ? t.add(q * 0x90) : ptr(0));         // quantisation tables 0, 1
        b.add(0x50 + q * 4).writePointer(q < 2 ? t.add(0x200 + q * 0x120) : ptr(0));  // DC Huffman 0, 1
        b.add(0x60 + q * 4).writePointer(q < 2 ? t.add(0x600 + q * 0x120) : ptr(0));  // AC Huffman 0, 1
      }
      fx['t' + i] = t;
    }
    fx['c' + i] = b;
    return b;
  }

  // Temporarily replaces a function of the exe for the length of one key (an Interceptor.replace that reverts from
  // setTimeout(0), with a token so a key's timer only undoes its own install). Used for jinit_compress_master, which the
  // game never calls except while saving a screenshot as a JPEG.
  function standIn(addr, cb) {
    const at = ptr(addr);
    const live = globalThis.__c3dmRepl || (globalThis.__c3dmRepl = {});
    const k = at.toString();
    if (live[k]) { Interceptor.revert(at); delete live[k]; }
    Interceptor.replace(at, cb);
    const tok = (globalThis.__c3dmTok = (globalThis.__c3dmTok || 0) + 1);
    live[k] = tok;
    setTimeout(function () { if (live[k] === tok) { Interceptor.revert(at); delete live[k]; } }, 0);
  }

  function jFixture(rows, stub) {
    return function () {
      const e = jEnv();
      const fx = { rec: e.rec, jrec: e.rec };
      rows.forEach(function (o, i) { jMake(e, o, fx, i); });
      if (stub) standIn(0x004b03a0, e.jinit);
      return fx;
    };
  }

  // jpeg_default_colorspace: every in_color_space the jump table and the default arm see, comp_info present
  const DCS = [0, 1, 2, 3, 4, 5, 6, -1, 100, 0x7fffffff, -0x80000000, 3].map(function (cs, i) {
    return { state: i === 11 ? 0x65 : 0x64, cs: cs, comp: true };
  });
  // jpeg_set_defaults: all six colour spaces with no comp_info (the allocation arm), comp_info present, preset tables, the
  // default colour-space error, and wrong global states
  const SD = [
    { state: 0x64, cs: 0, pool: true }, { state: 0x64, cs: 1, pool: true }, { state: 0x64, cs: 2, pool: true },
    { state: 0x64, cs: 3, pool: true }, { state: 0x64, cs: 4, pool: true }, { state: 0x64, cs: 5, pool: true },
    { state: 0x64, cs: 2, pool: true, comp: true }, { state: 0x64, cs: 3, pool: true, comp: true, tables: true },
    { state: 0x64, cs: 6, pool: true }, { state: 0x64, cs: -1, pool: true, comp: true },
    { state: 0x65, cs: 2, pool: true }, { state: 0x63, cs: 1, pool: true, comp: true },
    { state: 0, cs: 3, pool: true }, { state: 0x64, cs: 2, pool: true, tables: true },
    { state: 0x64, cs: 5, pool: true, comp: true }, { state: 0x66, cs: 4, pool: true, tables: true, comp: true },
  ];
  // jpeg_write_scanlines: state, image_height / next_scanline, progress monitor, pass_startup flag, controller mode
  const WS = [
    { state: 0x65, h: 10, next: 0 }, { state: 0x65, h: 10, next: 0, prog: true },
    { state: 0x65, h: 10, next: 0, startup: 1 }, { state: 0x65, h: 10, next: 8, prog: true, startup: 1 },
    { state: 0x65, h: 10, next: 10 }, { state: 0x65, h: 10, next: 12, prog: true },
    { state: 0x64, h: 10, next: 0 }, { state: 0x66, h: 10, next: 2, prog: true },
    { state: 0x65, h: 10, next: 0, mode: 1 }, { state: 0x65, h: 10, next: 0, mode: 2 },
    { state: 0x65, h: 10, next: 3, mode: 3 }, { state: 0, h: 10, next: 0 },
    { state: 0x65, h: 0, next: 0 }, { state: 0x65, h: 0x7fffffff, next: 0x7ffffff0, mode: 0 },
  ];
  // jpeg_start_compress: state, raw_data_in, preset tables (jpeg_suppress_tables marks them), progress irrelevant
  const SC = [
    { state: 0x64 }, { state: 0x64, raw: 1 }, { state: 0x64, tables: true }, { state: 0x64, raw: 1, tables: true },
    { state: 0x64, raw: -1, tables: true }, { state: 0x65 }, { state: 0x66, raw: 1, tables: true },
    { state: 0x63, tables: true }, { state: 0, raw: 5 }, { state: 0x64, raw: 0, next: 99, tables: true },
    { state: 0x67, raw: 1 }, { state: 0x64, raw: 2, h: 77 },
  ];
  // jpeg_finish_compress: state, scanlines done, pass structure (last = already the last pass, limit = passes before the
  // last, rows = total_iMCU_rows, period = a coefficient-controller suspension every period-th call), progress
  const FC = [
    { state: 0x65, h: 10, next: 10, last: 1 }, { state: 0x66, h: 10, next: 10, last: 1 },
    { state: 0x65, h: 10, next: 4, last: 1 }, { state: 0x67, h: 10, next: 2, last: 1 },
    { state: 0x65, h: 10, next: 10, last: 0, limit: 2, rows: 3 }, { state: 0x65, h: 10, next: 10, last: 0, limit: 2, rows: 3, prog: true },
    { state: 0x64, h: 10, next: 10, last: 1 }, { state: 0x65, h: 10, next: 10, last: 0, limit: 1, rows: 4, prog: true, period: 3 },
    { state: 0x65, h: 10, next: 10, last: 0, limit: 0, rows: 0 }, { state: 0x66, h: 10, next: 10, last: 0, limit: 3, rows: 2 },
    { state: 0x67, h: 5, next: 5, last: 0, limit: 1, rows: 1, prog: true, period: 1 }, { state: 0x63, h: 10, next: 10, last: 0, limit: 1, rows: 2 },
  ];

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dm_jdcs: jFixture(DCS, false),
    c3dm_jdef: jFixture(SD, false),
    c3dm_jwrite: jFixture(WS, false),
    c3dm_jstart: jFixture(SC, true),
    c3dm_jfinish: jFixture(FC, false),
  });

  // ======================================================================================================
  // Part 4 - the Window mouse dispatchers (shim/src/re/c3dm.cpp part 4). Synthetic windows (0x280 bytes) sharing ONE
  // fake vtable whose hit / miss / release slots of all seven siblings are recording stubs, plus the cdecl callbacks at
  // +0x23c .. +0x258 and panels (0x20 bytes, a second fake vtable with only slot 0x1c). The first thread is frozen: the
  // functions write g_curWidget 0x0083ab2c and read the global callback 0x0083ab04, both saved and restored at the
  // flush. Call log $rec: +0 count, then up to 48 entries {tag, id, a, b}; id is the dword at +4 of the object the stub
  // ran on (0 for the cdecl callbacks).
  const HIT_SLOTS = [0x34, 0x30, 0x3c, 0x40, 0x48, 0x38, 0x4c];
  const MISS_SLOTS = [0x98, 0x9c, 0x74, 0x78, 0x7c, 0x80, 0x88];
  const REL_SLOTS = [0xbc, 0xc0, 0xa4, 0xa8, 0xac, 0xb0, 0xb8];
  const CB_OFFS = [0x23c, 0x240, 0x244, 0x248, 0x24c, 0x250, 0x258];
  const PANEL_OFFS = [0x20, 0x24, 0x28, 0x2c, 0x34, 0x44, 0x48, 0x54, 0x58, 0x5c, 0x60, 0x68, 0x6c, 0x70];

  function winFixture(withGlobal) {
    return function () {
      __freeze.thread();
      __freeze.save(ptr(0x0083ab2c), 4);
      __freeze.save(ptr(0x0083ab04), 4);
      const recSize = 4 + 48 * 16;
      const rec = alloc(recSize);
      const log = function (tag, id, a, b) {
        const n = rec.readS32();
        if (n < 48) {
          const e = rec.add(4 + n * 16);
          e.writeS32(tag); e.add(4).writeS32(id); e.add(8).writeS32(a); e.add(12).writeS32(b);
        }
        rec.writeS32(n + 1);
      };
      const id = function (p) { return p.isNull() ? 0 : p.add(4).readS32(); };
      const cb = function (f, ret, args, abi) { return keep(new NativeCallback(f, ret, args, abi)); };
      const vt = alloc(0x200);
      HIT_SLOTS.forEach(function (s) {
        vt.add(s).writePointer(cb(function (t, a, b, r) {
          log(s, id(t), a, b); log(s + 0x1000, id(t), r.readS32(), r.add(4).readS32());
          return t.add(0xc).readS32();
        }, 'int', ['pointer', 'int', 'int', 'pointer'], 'thiscall'));
      });
      MISS_SLOTS.concat(REL_SLOTS).forEach(function (s) {
        vt.add(s).writePointer(cb(function (t, x, y) { log(s, id(t), x, y); return 0; }, 'int', ['pointer', 'int', 'int'], 'thiscall'));
      });
      const pvt = alloc(0x40);
      pvt.add(0x1c).writePointer(cb(function (t) { log(0x1c, id(t), 0, 0); return 0; }, 'int', ['pointer'], 'thiscall'));
      const cbs = {};
      CB_OFFS.forEach(function (o) {
        cbs[o] = cb(function (x, y) { log(0x2000 + o, 0, x, y); return 0; }, 'int', ['int', 'int'], 'default');
      });
      const globalCb = cb(function (x, y) { log(0x3000, 0, x, y); return 0; }, 'int', ['int', 'int'], 'default');
      ptr(0x0083ab04).writePointer(withGlobal ? globalCb : ptr(0));

      const panel = function (pid) { const p = alloc(0x20); p.writePointer(pvt); p.add(4).writeS32(pid); return p; };
      const hot = function (w, entries) {
        const arr = alloc(0x20 * Math.max(entries.length, 1));
        entries.forEach(function (e, i) {
          const p = arr.add(i * 0x20);
          [e.l, e.t, e.r, e.b].forEach(function (v, k) { p.add(4 + 4 * k).writeS32(v); });
          p.add(0x14).writeS32(e.second); p.add(0x18).writeS32(e.first);
        });
        w.add(0xbc + 0x50).writePointer(arr); w.add(0xbc + 0x58).writeS32(entries.length);
      };
      // o: flags (+0x9c), a0 (+0xa0), hit (what the hit slots return), cbs, panels
      const win = function (wid, o) {
        const w = alloc(0x280);
        w.writePointer(vt); w.add(4).writeS32(wid);
        w.add(0xc).writeS32(o.hit || 0);
        if (o.flags) w.add(0x9c).writeU32(o.flags);
        if (o.a0) w.add(0xa0).writeU8(o.a0);
        if (o.cbs) CB_OFFS.forEach(function (c) { w.add(c).writePointer(cbs[c]); });
        if (o.panels) PANEL_OFFS.forEach(function (c) { w.add(c).writePointer(panel(wid * 0x100 + c)); });
        return w;
      };
      const HOT = [{ l: 0, t: 0, r: 100, b: 100, first: 11, second: 22 }, { l: 50, t: 50, r: 150, b: 150, first: 33, second: 44 }];
      const fx = { obj: rec, rec: rec };
      fx.ignA = win(1, { flags: 0x200000, cbs: true, panels: true, hit: 1 }); hot(fx.ignA, HOT);
      fx.ignB = win(2, { a0: 0x08, cbs: true, panels: true, hit: 1 }); hot(fx.ignB, HOT);
      fx.plain = win(3, { cbs: true, panels: true, hit: 1 }); hot(fx.plain, HOT);
      fx.miss = win(4, { cbs: true, panels: true, hit: 0 }); hot(fx.miss, HOT);
      fx.bare = win(5, {}); hot(fx.bare, []);
      fx.onlyCb = win(6, { cbs: true, hit: 1 }); hot(fx.onlyCb, HOT);
      fx.onlyPanel = win(7, { panels: true, hit: 0 }); hot(fx.onlyPanel, HOT);
      fx.both = win(8, { flags: 0x200000, a0: 0x08, cbs: true, panels: true }); hot(fx.both, HOT);
      return fx;
    };
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dm_win: winFixture(true),
    c3dm_win_noglobal: winFixture(false),
  });

  // ======================================================================================================
  // Part 5 - thin wrappers over a device object. Every device here is a fixture object with a recording vtable
  // (NativeCallbacks pushed onto __diffKeepAlive); $rec is the shared recorder (hash, count, last value).
  //   c3dm_dev  : Widget_clearArea (device at +4, slot 0x44), Snd::queryDevice4845d0 / Snd::status (device at +0x40, slots
  //               0x5c / 0x20), Palette::setEntries (device at +4, slot 0x14; the palette's own slot 0 creates one).
  //               Nothing live is touched.
  //   c3dm_surf : Surface::releaseBacking / createFromId / createSized, which read the display global 0x0083ad50: the first
  //               thread is frozen and the global is swapped for a fixture display whose slots 0x74 (create handle) and
  //               0x88 (release) record, and which hands out fixture handles (slots 4 and 8 record and compute).
  function devEnv() {
    const rec = mkRec();
    const mk = function (ret, args, fn) { return keep(new NativeCallback(fn, ret, args, 'thiscall')); };
    const id = function (p) { return p.isNull() ? 0 : p.add(4).readS32(); };
    const e = { rec: rec, id: id, mk: mk };
    e.dvt = alloc(0x200);
    e.dvt.add(0x44).writePointer(mk('int', ['pointer', 'int', 'int'], function (t, a, b) {
      push(rec, 0x44000000 + (id(t) & 0xffff)); push(rec, a >>> 0); push(rec, b >>> 0); return (id(t) + b) | 0;
    }));
    e.dvt.add(0x5c).writePointer(mk('int', ['pointer'], function (t) { push(rec, 0x5c000000 + (id(t) & 0xffff)); return (id(t) * 7 + 3) | 0; }));
    e.dvt.add(0x20).writePointer(mk('int', ['pointer'], function (t) { push(rec, 0x20000000 + (id(t) & 0xffff)); return (id(t) * 5 - 1) | 0; }));
    e.dvt.add(0x14).writePointer(mk('int', ['pointer', 'pointer', 'int', 'int'], function (t, buf, start, count) {
      push(rec, 0x14000000 + (id(t) & 0xffff)); push(rec, start >>> 0); push(rec, count >>> 0);
      if (count > 0) {
        const bytes = new Uint8Array(buf.readByteArray(3 * count));
        let h = 0;
        for (let i = 0; i < bytes.length; i++) h = (Math.imul(h, 31) + bytes[i]) >>> 0;
        push(rec, h);
      }
      return (count * 3 + start) | 0;
    }));
    e.dev = function (n) { const d = alloc(0x20); d.writePointer(e.dvt); d.add(4).writeS32(n); return d; };
    return e;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3dm_dev() {
      const e = devEnv();
      const fx = { rec: e.rec };
      // twelve widgets / sound objects: the device at +4 / +0x40 is null in three of them
      const ids = [11, 22, -3, 0, 5, 1000, 7, 8, 9, 10, 0x7fff, -1];
      fx.wblk = alloc(12 * 0x20, 0x4d4d4d4d);
      fx.sblk = alloc(12 * 0x80, 0x4d4d4d4d);
      for (let i = 0; i < 12; i++) {
        const dev = (i % 4 === 3) ? ptr(0) : e.dev(ids[i]);
        const w = fx.wblk.add(i * 0x20); w.add(4).writePointer(dev); fx['w' + i] = w;
        const s = fx.sblk.add(i * 0x80); s.add(0x40).writePointer(dev); fx['s' + i] = s;
      }
      // palettes: slot 0 creates a device for the palette it runs on (stores it at +4); the table has a 0x28-byte header
      // and 64 four-byte entries
      const pdevs = [];
      for (let i = 0; i < 6; i++) pdevs.push(e.dev(500 + i));      // made now, so both arms store the SAME pointer
      const pvt = alloc(0x40);
      pvt.writePointer(e.mk('int', ['pointer'], function (t) {
        push(e.rec, 0xc0000000 + (e.id(t) & 0xffff));
        t.add(4).writePointer(pdevs[t.add(8).readS32() - 500]);
        return 0;
      }));
      fx.pblk = alloc(6 * 0x20, 0x4d4d4d4d);
      for (let i = 0; i < 6; i++) {
        const p = fx.pblk.add(i * 0x20);
        p.writePointer(pvt); p.add(4).writePointer(i % 2 ? e.dev(100 + i) : ptr(0)); p.add(8).writeS32(500 + i);
        fx['p' + i] = p;
      }
      const tbl = alloc(0x28 + 256 * 4);
      for (let i = 0; i < 0x28 + 256 * 4; i++) tbl.add(i).writeU8((i * 37 + 11) & 0xff);
      fx.tbl = tbl;
      fx.obj = fx.wblk;
      return fx;
    },

    c3dm_surf() {
      const rec = mkRec();
      const mk = function (ret, args, fn) { return keep(new NativeCallback(fn, ret, args, 'thiscall')); };
      const id = function (p) { return p.isNull() ? 0 : p.add(4).readS32(); };
      // handles: slot 4 (w, h, a, b), slot 8 (id, a, b)
      const hvt = alloc(0x40);
      hvt.add(4).writePointer(mk('int', ['pointer', 'int', 'int', 'int', 'int'], function (t, w, h, a, b) {
        push(rec, 0x04000000 + (id(t) & 0xffff)); push(rec, w >>> 0); push(rec, h >>> 0); push(rec, a >>> 0); push(rec, b >>> 0);
        return (w * 3 + h - a + b) | 0;
      }));
      hvt.add(8).writePointer(mk('int', ['pointer', 'int', 'int', 'int'], function (t, i, a, b) {
        push(rec, 0x08000000 + (id(t) & 0xffff)); push(rec, i >>> 0); push(rec, a >>> 0); push(rec, b >>> 0);
        return (i * 5 + a - b) | 0;
      }));
      const handles = [];
      for (let i = 0; i < 4; i++) { const h = alloc(0x20); h.writePointer(hvt); h.add(4).writeS32(0x700 + i); handles.push(h); }
      const dvt = alloc(0x200);
      dvt.add(0x74).writePointer(mk('pointer', ['pointer', 'pointer'], function (t, slot) {
        const n = rec.add(0x10).readU32();                        // our creation counter (state)
        rec.add(0x10).writeU32(n + 1);
        push(rec, 0x74000000 + n); push(rec, slot.toInt32() & 0xff);
        return handles[n % 4];
      }));
      dvt.add(0x88).writePointer(mk('int', ['pointer', 'pointer'], function (t, h) {
        push(rec, 0x88000000 + (id(h) & 0xffff));
        return 0;
      }));
      const disp = alloc(0x40);
      disp.writePointer(dvt);
      const fx = { rec: rec, disp: disp };
      // twelve surfaces (0x30 bytes, pre-filled 0x5a5a5a5a): a live handle at +4 in the odd ones, junk in +8 .. +0x1c
      fx.sblk = alloc(12 * 0x30, 0x5a5a5a5a);
      for (let i = 0; i < 12; i++) {
        const s = fx.sblk.add(i * 0x30);
        s.add(4).writePointer(i % 2 ? handles[i % 4] : ptr(0));
        s.add(8).writeS32(i); s.add(0xc).writeS32(0x40 + i); s.add(0x10).writeS32(0x50 + i);
        s.add(0x18).writeS32(0x60 + i); s.add(0x1c).writeS32(0x70 + i);
        fx['s' + i] = s;
      }
      fx.obj = fx.sblk;
      __freeze.thread();
      __freeze.save(ptr(0x0083ad50), 4);
      ptr(0x0083ad50).writePointer(disp);
      return fx;
    },
  });

  // ======================================================================================================
  // Part 6 - the Snd wrappers (shim/src/re/c3dm.cpp part 6). Globals the keys write (all saved and restored by freeze.js
  // under a frozen first thread): the sound function table 0x0083af6c.. (entry 0 create, entry 1 release), the list tail /
  // head 0x0083af64 / 0x0083af68, the library flag 0x0083afc4 and, for startSoundSlot, the first 12 slot objects at
  // 0x0080d840 (108 bytes each; only their vtable pointer at +0 is replaced). Nothing reaches sound.dll.
  function sndEnv() {
    const rec = mkRec();
    const mk = function (ret, args, fn, abi) { return keep(new NativeCallback(fn, ret, args, abi)); };
    const e = { rec: rec, mk: mk };
    const id = function (p) { return p.isNull() ? 0 : p.add(4).readS32(); };
    const svt = alloc(0x200);
    svt.add(4).writePointer(mk('int', ['pointer', 'int'], function (t, a) { push(rec, 0x04000000 + (id(t) & 0xffff)); push(rec, a >>> 0); return (a & 1) ? a : 0; }, 'thiscall'));
    svt.add(0x28).writePointer(mk('int', ['pointer'], function (t) { push(rec, 0x28000000 + (id(t) & 0xffff)); return 0x2800 + id(t); }, 'thiscall'));
    svt.add(0x54).writePointer(mk('int', ['pointer', 'int'], function (t, a) { push(rec, 0x54000000 + (id(t) & 0xffff)); push(rec, a >>> 0); return (a & 2) ? (a * 3) | 0 : 0; }, 'thiscall'));
    svt.add(0x80).writePointer(mk('int', ['pointer'], function (t) { push(rec, 0x80000000 + (id(t) & 0xffff)); return 0; }, 'thiscall'));
    svt.add(0x88).writePointer(mk('int', ['pointer', 'int', 'int'], function (t, a, m) { push(rec, 0x88000000 + (id(t) & 0xffff)); push(rec, a >>> 0); push(rec, m >>> 0); return 0; }, 'thiscall'));
    svt.add(0x8c).writePointer(mk('int', ['pointer'], function (t) { push(rec, 0x8c000000 + (id(t) & 0xffff)); return 0x8c00 + id(t); }, 'thiscall'));
    const dvt = alloc(0x40);
    dvt.add(0x14).writePointer(mk('int', ['pointer'], function (t) { push(rec, 0x14000000 + (id(t) & 0xffff)); return 0x1400 + id(t); }, 'thiscall'));
    e.svt = svt;
    e.dev = function (n) { const d = alloc(0x20); d.writePointer(dvt); d.add(4).writeS32(n); return d; };
    return e;
  }

  // lib: library flag value; table: install the cdecl recorders in the function table
  function sndTableFixture(lib, table) {
    return function () {
      const e = sndEnv();
      __freeze.thread();
      __freeze.save(ptr(0x0083af64), 0x68);
      ptr(0x0083afc4).writeU32(lib);
      if (table) {
        ptr(0x0083af6c).writePointer(e.mk('int', ['int', 'int', 'int'], function (a, b, c) {
          push(e.rec, 0x6c000000 + (a & 0xffff)); push(e.rec, b >>> 0); push(e.rec, c >>> 0); return (a + 2 * b - c) | 0;
        }, 'default'));
        ptr(0x0083af70).writePointer(e.mk('int', ['int'], function (p) {
          push(e.rec, 0x70000000 + (p & 0xffff)); return (p * 3 + 1) | 0;
        }, 'default'));
      }
      return { rec: e.rec };
    };
  }

  // clearSoundPtrs: the 11 table dwords (and the guard dwords around them) pre-filled from a seed
  function sndClearFixture(seed) {
    return function () {
      __freeze.thread();
      __freeze.save(ptr(0x0083af64), 0x68);
      for (let i = 0; i < 0x68; i += 4) ptr(0x0083af64).add(i).writeU32((Math.imul(seed + 1, 0x9e3779b1) + i * 0x01010101) >>> 0);
      return {};
    };
  }

  // synthetic Snd objects (0x80 bytes) for stop / applyField / applyMode: a device at +0x40 in the odd ones, flags at +0x44
  const SND_FLAGS = [1, 0, 0xffffffff, 3, 0x100, 0x101, 2, 0xfe, 5, 0x80000001, 0x10, 0xf1];
  function sndObjFixture() {
    const e = sndEnv();
    const fx = { rec: e.rec };
    fx.oblk = alloc(12 * 0x80, 0x4d4d4d4d);
    for (let i = 0; i < 12; i++) {
      const o = fx.oblk.add(i * 0x80);
      o.writePointer(e.svt); o.add(4).writeS32(0x100 + i);
      o.add(0x40).writePointer(i % 2 ? e.dev(0x200 + i) : ptr(0));
      o.add(0x44).writeU32(SND_FLAGS[i]);
      fx['o' + i] = o;
    }
    fx.obj = fx.oblk;
    return fx;
  }

  // the global doubly linked list: head 0x0083af68, tail 0x0083af64; node flag bit 2 means "linked"
  function sndListFixture(flagsList) {
    return function () {
      const e = sndEnv();
      __freeze.thread();
      __freeze.save(ptr(0x0083af64), 8);
      const fx = { rec: e.rec };
      const n = flagsList.length;
      fx.nblk = alloc(8 * 0x80, 0x4d4d4d4d);
      const nodes = [];
      for (let i = 0; i < n; i++) {
        const o = fx.nblk.add(i * 0x80);
        o.writePointer(e.svt); o.add(4).writeS32(0x300 + i);
        o.add(0x44).writeU32(flagsList[i]);
        o.add(0x48).writePointer(ptr(0)); o.add(0x4c).writePointer(ptr(0));
        nodes.push(o); fx['n' + i] = o;
      }
      for (let i = 0; i < n; i++) {
        if (i > 0) nodes[i].add(0x48).writePointer(nodes[i - 1]);
        if (i + 1 < n) nodes[i].add(0x4c).writePointer(nodes[i + 1]);
      }
      ptr(0x0083af68).writePointer(nodes[0]);
      ptr(0x0083af64).writePointer(nodes[n - 1]);
      // two unlinked objects: flag bit 2 clear
      fx.x0 = fx.nblk.add(6 * 0x80); fx.x0.add(0x44).writeU32(1);
      fx.x1 = fx.nblk.add(7 * 0x80); fx.x1.add(0x44).writeU32(0xfd);
      fx.obj = fx.nblk;
      return fx;
    };
  }

  // startSoundSlot: the first 12 live slot objects get the fixture vtable (the rest of each object is untouched and
  // restored at the flush); the slot index comes from the object's address in the stub
  function sndSlotsFixture() {
    const e = sndEnv();
    const rec = e.rec;
    __freeze.thread();
    __freeze.save(ptr(0x0080d840), 12 * 0x6c);
    const sv = alloc(0x200);
    const idx = function (t) { return Math.floor((t.toUInt32() - 0x0080d840) / 0x6c); };
    sv.add(4).writePointer(e.mk('int', ['pointer', 'int'], function (t, a) {
      push(rec, 0x04000000 + idx(t)); push(rec, a >>> 0); return (idx(t) % 3 === 0) ? 0 : (a + idx(t)) | 0;
    }, 'thiscall'));
    sv.add(0x28).writePointer(e.mk('int', ['pointer'], function (t) { push(rec, 0x28000000 + idx(t)); return 0x2800 + idx(t); }, 'thiscall'));
    for (let i = 0; i < 12; i++) ptr(0x0080d840).add(i * 0x6c).writePointer(sv);
    return { rec: rec };
  }

  const F6 = { c3dm_snd_lib: sndTableFixture(0x1234, true), c3dm_snd_nolib: sndTableFixture(0, true),
               c3dm_snd_objs: sndObjFixture, c3dm_snd_slots: sndSlotsFixture,
               c3dm_snd_list5: sndListFixture([3, 0xffffffff, 2, 0x1f2, 0x0a]),
               c3dm_snd_list2: sndListFixture([2, 0x82]), c3dm_snd_list1: sndListFixture([0xffffffff]) };
  for (let s = 0; s < 10; s++) F6['c3dm_snd_clear_' + s] = sndClearFixture(s);
  Object.assign(globalThis.DIFF_FIXTURES, F6);
})();
