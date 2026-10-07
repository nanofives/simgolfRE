// Fixtures for C3 batch c3u (listFindById, ListModel::selectedId, charEditorZone, comboCurrentIndex,
// setWidgetScalar, setHudTextSlot, Button::setMode, Button::setColorHover). Only our own synthetic values (no game
// text). Names are prefixed c3u_. Allocations go through __keepAlloc (diff_hook rewrites Memory.alloc) so blocks
// referenced only from a field are not freed mid-run; NativeCallbacks are pushed onto __diffKeepAlive by hand.
(function () {
  function zero(p, size) { for (let i = 0; i < size; i += 4) p.add(i).writeU32(0); }
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  // A record buffer shared by the virtual-call recorders: +0 call count, +4 last object id, +8 running hash of every
  // recorded value (hash = hash * 31 + v); so call order and arguments are reflected in the state regions.
  function record(rec, id, vals) {
    rec.writeU32(rec.readU32() + 1);
    rec.add(4).writeU32(id);
    let h = rec.add(8).readU32();
    h = (Math.imul(h, 31) + id) >>> 0;
    for (let i = 0; i < vals.length; i++) { h = (Math.imul(h, 31) + (vals[i] >>> 0)) >>> 0; }
    rec.add(8).writeU32(h);
  }
  // A list node (List489 layout): +0x4 id, +0xc next, +0x10 prev. Returns the node pointer.
  function node(id) { const n = Memory.alloc(0x14); zero(n, 0x14); n.add(4).writeS32(id); return n; }
  // Build a singly- (or circular doubly-) linked list of the ids into obj at `base` (head +0, cursor +4, count +8,
  // index +0xc). When circular, each node's +0x10 prev is set so negative seeks work.
  function buildList(obj, base, ids, circular) {
    const ns = ids.map(node);
    for (let i = 0; i < ns.length; i++) {
      const nx = ns[(i + 1) % ns.length];
      if (circular || i + 1 < ns.length) ns[i].add(0xc).writePointer(nx);
      if (circular) ns[i].add(0x10).writePointer(ns[(i - 1 + ns.length) % ns.length]);
    }
    obj.add(base).writePointer(ns[0]);          // head
    obj.add(base + 4).writePointer(ns[0]);      // cursor (init = head)
    obj.add(base + 8).writeS32(ids.length);     // count
    obj.add(base + 0xc).writeS32(0);            // index
    return ns;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // listFindById 0x004a4890: list at this+0x80, five nodes ids 10,20,30,40,50.
    c3u_list() {
      const obj = Memory.alloc(0x100); zero(obj, 0x100);
      buildList(obj, 0x80, [10, 20, 30, 40, 50], false);
      return { obj: obj };
    },

    // listFindById 0x004a4890 head == 0 guard: null head at +0x80, with the index field +0x8c preset to 7 so the
    // returned value is defined (and not 0).
    c3u_list_empty() {
      const obj = Memory.alloc(0x100); zero(obj, 0x100);
      obj.add(0x8c).writeS32(7);
      return { obj: obj };
    },

    // ListModel::selectedId 0x00489950: eleven objects, each a circular 6-node list (ids 100..600) at +0xc8 with a
    // different selection index +0xf0 reaching a distinct node or branch side. o10 has a null head; the others exercise
    // forward, out-of-range (seek skipped), backward, and abs-over-count (seek returns early) seeks.
    c3u_sel() {
      const fx = {};
      const sels = [0, 1, 2, 4, 5, 6, -1, -3, -5, -7, 0];   // o10 (index 10) is the null-head object; its sel is unused
      for (let i = 0; i < 11; i++) {
        const o = Memory.alloc(0x100); zero(o, 0x100);
        buildList(o, 0xc8, [100, 200, 300, 400, 500, 600], true);
        o.add(0xf0).writeS32(sels[i]);
        if (i === 10) { o.add(0xc8).writePointer(ptr(0)); o.add(0xcc).writePointer(ptr(0)); }
        fx['o' + i] = o;
      }
      fx.obj = fx.o0;
      return fx;
    },

    // comboCurrentIndex 0x004942a0: six objects. Byte +4 bit 2 chooses the list; vbtables at +0x1488 (bit set, S494 at
    // +0x1548: count +0x1550, item +0x1554) and +0x2d98 (bit clear, S494 at +0x2e58: count +0x2e60, item +0x2e64), each
    // vbtable's +8 = 0. An item node holds its id at +4.
    c3u_combo() {
      const fx = {};
      // [bit2set, hasItem, id]
      const cfg = [[true, true, 111], [true, false, 0], [false, true, 222], [false, false, 0],
                   [true, true, 333], [false, true, 444], [true, true, 555], [false, true, 666],
                   [true, true, 777], [false, true, 888], [true, true, 999]];
      for (let i = 0; i < 11; i++) {
        const o = Memory.alloc(0x2e70); zero(o, 0x2e70);
        const vtb = Memory.alloc(0x10); zero(vtb, 0x10);
        const vta = Memory.alloc(0x10); zero(vta, 0x10);
        o.add(0x1488).writePointer(vtb);
        o.add(0x2d98).writePointer(vta);
        const [bit, has, id] = cfg[i];
        o.add(4).writeU8(bit ? 4 : 0);
        if (bit) {
          o.add(0x1550).writeS32(has ? 1 : 0);
          if (has) o.add(0x1554).writePointer(node(id));
        } else {
          o.add(0x2e60).writeS32(has ? 1 : 0);
          if (has) o.add(0x2e64).writePointer(node(id));
        }
        fx['o' + i] = o;
      }
      fx.obj = fx.o0;
      return fx;
    },

    // setWidgetScalar 0x004967f0: clamp range [10, 100]; vtable +0x120 records the invalidate call into rec. The global
    // 0x0083ab2c receives obj+0x130 (a small dummy block). Base = mirror flag obj+0x588 off.
    c3u_scalar() { return scalar(0); },
    c3u_scalar_mirror() { return scalar(1); },

    // setHudTextSlot 0x00494cb0: synthetic strings t0..t3 for the slot buffers.
    c3u_hud() {
      const texts = ['', 'hud-a', 'notice two', 'a somewhat longer synthetic notification line 0123456789'];
      const fx = { obj: Memory.alloc(4) };
      for (let i = 0; i < texts.length; i++) fx['t' + i] = Memory.allocUtf8String(texts[i]);
      return fx;
    },

    // Button::setMode 0x004890e0: o has a parent control at +0x130, o2 does not. ONE fake self vtable (+0x120 recorder,
    // thiscall self) and ONE parent vtable (+0xd8 recorder, thiscall self + two dwords); both fold into rec. Current
    // mode +0x578 = 1, notify id +0x5e8 set.
    c3u_setmode() {
      const rec = Memory.alloc(0x40); zero(rec, 0x40);
      const vtSelf = Memory.alloc(0x200); zero(vtSelf, 0x200);
      vtSelf.add(0x120).writePointer(keep(new NativeCallback(function (self) {
        record(rec, self.add(0x578).readS32(), [0x120]);
      }, 'void', ['pointer'], 'thiscall')));
      const vtParent = Memory.alloc(0x200); zero(vtParent, 0x200);
      vtParent.add(0xd8).writePointer(keep(new NativeCallback(function (self, a, b) {
        record(rec, 0xd8, [a, b]);
      }, 'void', ['pointer', 'uint32', 'int'], 'thiscall')));
      const parent = Memory.alloc(0x40); zero(parent, 0x40); parent.writePointer(vtParent);
      const o = Memory.alloc(0x600); zero(o, 0x600);
      o.writePointer(vtSelf); o.add(0x130).writePointer(parent); o.add(0x578).writeS32(1); o.add(0x5e8).writeU32(0x555);
      const o2 = Memory.alloc(0x600); zero(o2, 0x600);
      o2.writePointer(vtSelf); o2.add(0x130).writePointer(ptr(0)); o2.add(0x578).writeS32(1); o2.add(0x5e8).writeU32(0x777);
      return { obj: o, o: o, o2: o2, rec: rec };
    },

    // Button::setColorHover 0x00488970: o is active (this+0x130 set, font this+0x274 with +4 = 0 so the palette helper
    // 0x004789f0 returns early); o2 is inactive (this+0x130 null). The quad is written at this+0x274+0x70/0x80/0x90/0xa0.
    c3u_hover() {
      const vt = Memory.alloc(0x40); zero(vt, 0x40);
      const dummy = Memory.alloc(0x40); zero(dummy, 0x40);
      const o = Memory.alloc(0x400); zero(o, 0x400);
      o.writePointer(vt); o.add(0x130).writePointer(dummy);    // active; font+4 (o+0x278) stays 0
      const o2 = Memory.alloc(0x400); zero(o2, 0x400);
      o2.writePointer(vt); o2.add(0x130).writePointer(ptr(0)); // inactive
      return { obj: o, o: o, o2: o2 };
    },
  });

  function scalar(mirror) {
    const rec = Memory.alloc(0x40); zero(rec, 0x40);
    const vt = Memory.alloc(0x200); zero(vt, 0x200);
    vt.add(0x120).writePointer(keep(new NativeCallback(function (self) {
      record(rec, self.add(0x58c).readS32(), [0x120]);
    }, 'void', ['pointer'], 'thiscall')));
    const dummy = Memory.alloc(0x40); zero(dummy, 0x40);
    const o = Memory.alloc(0x600); zero(o, 0x600);
    o.writePointer(vt);
    o.add(0x130).writePointer(dummy);
    o.add(0x580).writeS32(10);
    o.add(0x584).writeS32(100);
    o.add(0x588).writeS32(mirror);
    o.add(0x58c).writeS32(0);
    return { obj: o, rec: rec };
  }
})();
