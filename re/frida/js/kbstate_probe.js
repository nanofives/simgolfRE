const log = [];
Interceptor.attach(Module.getGlobalExportByName('GetKeyboardState'), {
  onEnter(a) { this.p = a[0]; },
  onLeave(r) { log.push([Date.now() % 100000, this.p.readU8() /*VK 0*/, this.p.add(1).readU8(), this.p.add(2).readU8()]); if (log.length > 400) log.shift(); }
});
const msgs = [];
rpc.exports = { log: () => log };
