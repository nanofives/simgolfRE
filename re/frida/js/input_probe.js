// Count input-related API calls (diagnostic only).
const counts = {};
for (const n of ['GetCursorPos', 'GetAsyncKeyState', 'GetKeyState', 'GetKeyboardState', 'ScreenToClient', 'ClientToScreen', 'SetCursorPos', 'ClipCursor', 'ShowCursor', 'SetWindowPos', 'MoveWindow']) {
  const p = Module.findGlobalExportByName(n);
  if (p) Interceptor.attach(p, { onEnter(a) { counts[n] = (counts[n] || 0) + 1; if (n === 'ClipCursor' || n === 'SetWindowPos') send(n + ' ' + a[0] + ' ' + (n === 'SetWindowPos' ? a[2].toInt32() + ',' + a[3].toInt32() + ' ' + a[4].toInt32() + 'x' + a[5].toInt32() + ' f=' + a[6] : (a[0].isNull() ? 'NULL' : a[0].readS32() + ',' + a[0].add(4).readS32() + '-' + a[0].add(8).readS32() + ',' + a[0].add(12).readS32()))); } });
}
rpc.exports = { counts: () => counts };
