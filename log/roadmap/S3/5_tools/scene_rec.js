// S3.5 recorder (appended to a pinned scenario's agent by log/roadmap/S3/5_tools/scene_rec.py): the root surface of the
// ORIGINAL at the return of renderSpriteQueue (golf_clean.exe 0x00463100; its only caller is mainLoop, once per frame in P4),
// i.e. after the frame's terrain and every queued sprite. The root surface's bits come from the DC Terrain.dll's initSystem
// receives (its third argument: the DC of the jgld surface at g_rootWindow + 0x278, S3.2's finding). Every return copies the
// bits natively into one buffer (the last one kept) and counts the call; srSnapNow(label), called by the scenario's
// checkpoint (the main thread waits in the clock stub), queues that copy with its call ordinal; srTake sends them.
(function () {
  const RSQ = ptr(0x00463100), INIT_SYSTEM_RVA = 0x9c80;
  let rootHdc = null, dib = null, buf = null, ord = 0, lastOrd = -1, hooked = false;
  const queue = [];
  function dibInfo() {
    if (dib || !rootHdc) return dib;
    const gdi = Process.getModuleByName('gdi32.dll');
    const GetCurrentObject = new NativeFunction(gdi.getExportByName('GetCurrentObject'), 'pointer', ['pointer', 'uint32'], 'stdcall');
    const GetObjectA = new NativeFunction(gdi.getExportByName('GetObjectA'), 'int', ['pointer', 'int', 'pointer'], 'stdcall');
    const hbm = GetCurrentObject(rootHdc, 7);
    if (hbm.isNull()) return null;
    const ds = Memory.alloc(96);
    if (GetObjectA(hbm, 84, ds) < 84) return null;
    const bits = ds.add(20).readPointer();
    if (bits.isNull()) return null;
    const wb = ds.add(12).readS32(), bh = ds.add(32).readS32();
    dib = { bits: bits, n: wb * Math.abs(bh), meta: { w: ds.add(4).readS32(), h: ds.add(8).readS32(), wb: wb,
            bpp: ds.add(18).readU16(), biHeight: bh } };
    buf = Memory.alloc(dib.n);
    return dib;
  }
  Interceptor.attach(RSQ, {
    onLeave() {
      ord += 1;
      if (!dibInfo()) return;
      Memory.copy(buf, dib.bits, dib.n);
      lastOrd = ord;
    }
  });
  function hookTerrain() {
    if (hooked) return;
    const tm = Process.findModuleByName('Terrain.dll');
    if (!tm) return;
    hooked = true;
    Interceptor.attach(tm.base.add(INIT_SYSTEM_RVA), { onEnter(a) { rootHdc = a[2]; } });
  }
  try { Process.attachModuleObserver({ onAdded: function () { hookTerrain(); } }); } catch (e) { }
  const iv = setInterval(function () { hookTerrain(); if (hooked) clearInterval(iv); }, 5);
  rpc.exports.srSnapNow = function (label) {
    if (!dib || lastOrd < 0) { queue.push({ label: label, none: 1, calls: ord }); return false; }
    queue.push({ label: label, ord: lastOrd, calls: ord, meta: dib.meta, data: buf.readByteArray(dib.n) });
    return true;
  };
  rpc.exports.srTake = function () {
    const n = queue.length;
    while (queue.length) {
      const s = queue.shift();
      const d = s.data;
      delete s.data;
      send({ sr: 'snap', meta: s }, d || null);
    }
    send({ sr: 'done' });
    return n;
  };
})();
