// Log BinkBufferOpen(hwnd, w, h, flags) and the buffer type Bink actually chose.
// binkw32 is not mapped yet when we attach at spawn, so hook on module load.
function hookBink(bink) {
  const getDesc = new NativeFunction(bink.getExportByName('_BinkBufferGetDescription@4'), 'pointer', ['pointer'], 'stdcall');
  Interceptor.attach(bink.getExportByName('_BinkBufferOpen@16'), {
    onEnter(a) {
      this.s = 'BinkBufferOpen hwnd=' + a[0] + ' ' + a[1].toInt32() + 'x' + a[2].toInt32() + ' flags=0x' + a[3].toString(16);
      send('enter ' + this.s);
      if (globalThis.BINK_FORCE !== undefined) { a[3] = ptr((a[3].toUInt32() & ~0x1f) | globalThis.BINK_FORCE); send('forced flags=0x' + a[3].toString(16)); }
    },
    onLeave(r) { let d = '?'; try { d = r.isNull() ? 'NULL' : getDesc(r).readAnsiString(); } catch (e) {} send(this.s + ' -> ' + r + ' "' + d + '"'); }
  });
  send('bink hooked');
}
const m = Process.findModuleByName('binkw32.dll');
if (m) hookBink(m);
else Process.attachModuleObserver({ onAdded(mod) { if (mod.name.toLowerCase() === 'binkw32.dll') hookBink(mod); } });
