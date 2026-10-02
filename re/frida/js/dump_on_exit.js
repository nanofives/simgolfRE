// Dump the golf.exe image (decrypted by SafeDisc at runtime) when ExitProcess fires, or on rpc call.
function dump(reason) {
  const m = Process.getModuleByName('golf.exe');
  const chunk = 0x10000;
  send({ type: 'begin', base: m.base.toString(), size: m.size, reason });
  for (let off = 0; off < m.size; off += chunk) {
    const n = Math.min(chunk, m.size - off);
    let buf;
    try { buf = m.base.add(off).readByteArray(n); } catch (e) { buf = new ArrayBuffer(n); }
    send({ type: 'chunk', off }, buf);
  }
  send({ type: 'end' });
  recv('ack', () => {}).wait();
}
Interceptor.attach(Module.getGlobalExportByName('ExitProcess'), { onEnter(args) { dump('ExitProcess'); } });
rpc.exports = { dump: () => dump('rpc') };
send({ type: 'ready' });
