// Fixtures for C3 batch c3z (HotList::add, wrapTextToWidth). Only our own synthetic values (no game text). Names are
// prefixed c3z_. Allocations go through Memory.alloc (diff_hook rewrites it to __keepAlloc so field-only references are
// not freed); NativeCallbacks are pushed onto __diffKeepAlive by hand.
(function () {
  function keep(cb) { globalThis.__diffKeepAlive.push(cb); return cb; }
  function zero(p, size) { for (let i = 0; i < size; i += 4) p.add(i).writeU32(0); }

  // HotList object: array base m_50 at this+0x50, capacity m_54 at this+0x54, count m_58 at this+0x58. The array holds
  // 20 entries of 0x20 bytes, all zeroed, so freeEntryTip on the reused slot frees nothing and add() never has to
  // grow(). add(..., s=NULL) also skips the malloc, so both A/B arms write identical bytes into slot 0.
  function hotlist() {
    const obj = Memory.alloc(0x100);
    zero(obj, 0x100);
    const arr = Memory.alloc(20 * 0x20);
    zero(arr, 20 * 0x20);
    obj.add(0x50).writePointer(arr);   // m_50 = array base
    obj.add(0x54).writeS32(20);        // m_54 = capacity
    obj.add(0x58).writeS32(0);         // m_58 = count
    return { obj: obj, arr: arr };
  }

  // wrapTextToWidth layout: a font pointer at layout+4; the font's vtable slot +0x10 is measureTextWidth's measure
  // callback measure(this, str, len) -> width. We return len*7 so widths are deterministic. Each case gets its own
  // string (our own synthetic ASCII) and its own budget cell (so a per-vector snapshot can hold distinct budgets); the
  // registry passes the byte count `remain` as a literal (0 for the empty-run case). The accumulator global at
  // 0x00839aa8 is seeded to `acc`: 0 reaches the acc==0 sides of the budget-exceeded tests, non-zero reaches the
  // other sides. The case list here mirrors _ACC0 / _ACCN in re/frida/registry.d/c3z.py (keep them in sync).
  function wrapCases(acc, texts) {
    const vt = Memory.alloc(0x20);
    zero(vt, 0x20);
    vt.add(0x10).writePointer(keep(new NativeCallback(function (self, str, len) {
      return (len * 7) | 0;
    }, 'int', ['pointer', 'pointer', 'int'], 'thiscall')));
    const font = Memory.alloc(8);
    zero(font, 8);
    font.writePointer(vt);             // font+0 = vtable
    const layout = Memory.alloc(0x10);
    zero(layout, 0x10);
    layout.add(4).writePointer(font);  // layout+4 = font
    const fx = { layout: layout };
    for (let i = 0; i < texts.length; i++) {
      const [text, budget] = texts[i];
      const s = Memory.alloc(text.length + 1);
      s.writeUtf8String(text);
      const b = Memory.alloc(4);
      b.writeS32(budget);
      fx['s' + i] = s;
      fx['b' + i] = b;
      if (i === 0) fx.obj = s;          // char* returns into s0 are reported as obj+offset
    }
    ptr('0x00839aa8').writeS32(acc);
    return fx;
  }

  // acc == 0: ten distinct runs. Spaces: "aaaa bbbb"@4, "aa bb cc"@2/@5, "a bb ccc dddd"@1/@4/@8, "xx yy"@2; "aaaa" has
  // none. Budgets reach: multi-word fit, first-word-too-wide (acc==0 -> return next word start), mid-run break,
  // single-word fit, single-word too narrow (acc==0 -> return 0), and the empty run (remain==0, registry vector).
  const _ACC0 = [["aaaa bbbb", 400], ["aaaa bbbb", 8], ["aa bb cc", 30], ["aaaa", 400], ["aaaa", 50],
                 ["a bb ccc dddd", 400], ["a bb ccc dddd", 20], ["xx yy", 400], ["xx yy", 10], ["aaaa", 8]];
  // acc != 0: the other side of the two budget-exceeded tests (first-word-too-wide and single-word too narrow return
  // the word itself), plus one fit that folds width into the non-zero accumulator.
  const _ACCN = [["aaaa bbbb", 8], ["aaaa", 8], ["aaaa bbbb", 400]];

  Object.assign(globalThis.DIFF_FIXTURES, {
    c3z_hotlist() { return hotlist(); },
    c3z_wrap_acc0() { return wrapCases(0, _ACC0); },
    c3z_wrap_accN() { return wrapCases(100, _ACCN); },
  });
})();
