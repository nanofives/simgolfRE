// Which virtual keys does the game poll, and through which API.
const seen = {};
for (const n of ['GetAsyncKeyState', 'GetKeyState']) {
  Interceptor.attach(Module.getGlobalExportByName(n), { onEnter(a) { const k = n + ':' + a[0].toInt32(); seen[k] = (seen[k] || 0) + 1; } });
}
Interceptor.attach(Module.getGlobalExportByName('GetKeyboardState'), { onEnter() { seen['GetKeyboardState'] = (seen['GetKeyboardState'] || 0) + 1; } });
rpc.exports = { seen: () => seen };
