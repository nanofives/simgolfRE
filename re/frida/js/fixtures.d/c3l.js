// Fixtures for C3 batch c3l (scanMarkupText, setButtonDefaults, MsgBox::setButtonB). Each seeds only our own
// synthetic values (no game text). Names are prefixed c3l_. Allocations go through __keepAlloc so blocks
// referenced only from a field are not garbage-collected mid-run.
Object.assign(globalThis.DIFF_FIXTURES, {
  // scanMarkupText 0x00476d40 scans a byte string for the first of { } [ ] $ (0x7b 0x7d 0x5b 0x5d 0x24),
  // decrementing the remaining-count *lenp. Buffer layout (obj = base, so the pointer return reads as
  // obj+offset): A B { C D [ E $ F } G ] H I J K L \0 with delimiters at indices 2,5,7,9,11. The p* keys are
  // start pointers into it; the len* cells are the initial remaining counts (restored after each vector).
  c3l_markup() {
    const buf = __keepAlloc(18);
    const bytes = [0x41, 0x42, 0x7b, 0x43, 0x44, 0x5b, 0x45, 0x24, 0x46, 0x7d,
                   0x47, 0x5d, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x00];
    for (let i = 0; i < bytes.length; i++) buf.add(i).writeU8(bytes[i]);
    const len = (v) => { const p = __keepAlloc(4); p.writeS32(v); return p; };
    return {
      obj: buf,
      p0: buf, p2: buf.add(2), p3: buf.add(3), p5: buf.add(5), p6: buf.add(6), p12: buf.add(12),
      len0: len(0), len1: len(1), len3: len(3), len8: len(8), len20: len(20),
    };
  },

  // setButtonDefaults 0x004889f0 / MsgBox::setButtonB 0x00490cf0 store the obj pointer into the first global
  // only when *(obj+4) is non-zero, then store the two scalar args unconditionally. obj has +4 non-zero (the
  // store branch runs); objz has +4 zero (that store is skipped). Both are 8-byte blocks; +0 is unread.
  c3l_btnobj() {
    const obj = __keepAlloc(8);
    obj.writeS32(0); obj.add(4).writeS32(0x1234);
    const objz = __keepAlloc(8);
    objz.writeS32(0); objz.add(4).writeS32(0);
    return { obj: obj, objz: objz };
  },
});
