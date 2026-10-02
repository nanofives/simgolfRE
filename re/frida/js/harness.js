// Always-loaded harness agent: drives the shim's virtual cursor and reports shim info.
// The shim is winmm.dll (proxy) next to the exe; its exports are only there once the loader maps it.
function shim() {
  const m = Process.findModuleByName('winmm.dll');
  if (!m || !m.findExportByName('SimGolfShim_GetInfo')) return null;
  return m;
}
let fns = null;
function bind() {
  if (fns) return fns;
  const m = shim();
  if (!m) return null;
  fns = {
    set: new NativeFunction(m.getExportByName('SimGolfShim_SetVirtualCursor'), 'void', ['int', 'int'], 'stdcall'),
    clear: new NativeFunction(m.getExportByName('SimGolfShim_ClearVirtualCursor'), 'void', [], 'stdcall'),
    info: new NativeFunction(m.getExportByName('SimGolfShim_GetInfo'), 'pointer', [], 'stdcall'),
    covPhase: new NativeFunction(m.getExportByName('SimGolfShim_CovPhase'), 'int', ['int'], 'stdcall'),
    covDump: new NativeFunction(m.getExportByName('SimGolfShim_CovDump'), 'int', ['pointer'], 'stdcall'),
    vkey: new NativeFunction(m.getExportByName('SimGolfShim_SetVirtualKey'), 'void', ['int', 'int'], 'stdcall'),
  };
  return fns;
}
rpc.exports = {
  shimInfo() { const f = bind(); return f ? f.info().readAnsiString() : null; },
  setCursor(x, y) { const f = bind(); if (!f) throw new Error('shim not loaded'); f.set(x, y); },
  clearCursor() { const f = bind(); if (f) f.clear(); },
  covPhase(n) { return bind().covPhase(n); },
  vkey(vk, state) { bind().vkey(vk, state); },
  covDump(path) { return bind().covDump(Memory.allocAnsiString(path)); },
};
