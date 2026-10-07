// Fixtures for C3 batch c3ad. Only our own synthetic buffer is built and a single vtable slot is overridden;
// no fake object is handed to the live renderer, no game text, no live list or device is replaced.
Object.assign(globalThis.DIFF_FIXTURES, {
  // build16BitColorTable 0x00461830 reads the LUT base pointer DAT_00824148 and the display object DAT_0083ad50,
  // whose vtable slot +0xb4 returns the pixel format (1 = RGB565, else RGB555). To exercise both format
  // branches safely:
  //   * DAT_00824148 is repointed at a private 0x60000-byte buffer (the five blocks reach 0x40000 + 0xffdf*2
  //     < 0x60000); it is the state region ($table). The live renderer that reads this LUT only ever sees
  //     valid full-size memory, so a stray concurrent read cannot fault.
  //   * the display OBJECT is left real; only its vtable slot +0xb4 is overridden with a NativeCallback that
  //     returns the chosen mode, so every other display method the menu renderer calls each frame still runs
  //     its real implementation on the real object. Forcing the mode only changes an int the blitter reads;
  //     writes stay in-bounds. The slot is not restored (no teardown hook), so the last run leaves a stub that
  //     returns a valid mode; this must run at the main menu.
  c3ad_build16(mode) {
    const display = ptr('0x0083ad50').readPointer();     // real display object
    const vtbl = display.readPointer();                  // its real vtable
    const modeCb = new NativeCallback(function () { return mode; }, 'int', []);
    globalThis.__diffKeepAlive.push(modeCb);             // NativeCallbacks are not rewritten to __keepAlloc
    Memory.protect(vtbl.add(0xb4), 4, 'rw-');
    vtbl.add(0xb4).writePointer(modeCb);                 // override only getPixelFormat (+0xb4)
    const table = Memory.alloc(0x60000);
    ptr('0x00824148').writePointer(table);              // DAT_00824148 = private LUT buffer
    return { obj: table, table: table };
  },
  c3ad_build16_565() { return globalThis.DIFF_FIXTURES.c3ad_build16(1); },
  c3ad_build16_555() { return globalThis.DIFF_FIXTURES.c3ad_build16(0); },
});
