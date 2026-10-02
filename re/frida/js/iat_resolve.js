// On ExitProcess: walk the IAT region and resolve every slot to module!symbol.
rpc.exports = {};
function resolve(start, end) {
  const out = [];
  for (let a = ptr(start); a.compare(ptr(end)) < 0; a = a.add(4)) {
    const v = a.readU32();
    if (v === 0) { out.push([a.toString(), 0, null, null]); continue; }
    const p = ptr(v);
    const m = Process.findModuleByAddress(p);
    const sym = DebugSymbol.fromAddress(p);
    out.push([a.toString(), v, m ? m.name : null, sym.name]);
  }
  return out;
}
Interceptor.attach(Module.getGlobalExportByName('ExitProcess'), { onEnter() {
  send({ iat: resolve(0x4ba000, 0x4ba400) });
  recv('ack', () => {}).wait();
} });
send('ready');
