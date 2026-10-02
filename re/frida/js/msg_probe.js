const log = [];
for (const n of ['DispatchMessageA', 'TranslateMessage']) {
  Interceptor.attach(Module.getGlobalExportByName(n), { onEnter(a) {
    const m = a[0].add(4).readU32();
    if (m >= 0x200 && m <= 0x20e) log.push(n[0] + ':' + m.toString(16) + '@' + a[0].add(12).readU32().toString(16));
  }});
}
rpc.exports = { log: () => log.splice(0) };
