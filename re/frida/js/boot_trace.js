// Boot tracer: logs registry / drive / file / window / exit APIs.
function s(p) { try { return p.isNull() ? 'NULL' : p.readAnsiString(); } catch (e) { return '?'; } }
function bt(ctx) { return Thread.backtrace(ctx, Backtracer.FUZZY).map(DebugSymbol.fromAddress).join('\n  '); }
const QUIET = /\.(bmp|tga|pcx|wav|chr|glf|pro)$/i;
const seenOnce = new Set();
const hooks = {
  RegOpenKeyExA: a => 'key=' + a[0] + ' sub=' + s(a[1]),
  RegQueryValueExA: a => 'h=' + a[0] + ' val=' + s(a[1]),
  RegCreateKeyExA: a => 'key=' + a[0] + ' sub=' + s(a[1]),
  RegSetValueExA: a => 'h=' + a[0] + ' val=' + s(a[1]),
  GetVolumeInformationA: a => s(a[0]),
  FindFirstFileA: a => s(a[0]),
  CreateFileA: a => s(a[0]),
  GetFileAttributesA: a => s(a[0]),
  LoadLibraryA: a => s(a[0]),
  MessageBoxA: a => s(a[1]) + ' | ' + s(a[2]),
  ChangeDisplaySettingsA: a => a[0].isNull() ? 'RESTORE flags=' + a[1] : ('bpp=' + a[0].add(0x68).readU32() + ' ' + a[0].add(0x6c).readU32() + 'x' + a[0].add(0x70).readU32() + ' flags=' + a[1]),
  DirectDrawCreate: a => 'guid=' + a[0],
  DirectDrawCreateEx: a => 'guid=' + a[0],
  wglCreateContext: a => 'hdc=' + a[0],
  ChoosePixelFormat: a => 'hdc=' + a[0] + ' bits=' + a[1].add(0x9).readU8(),
  SetWindowPos: a => 'hwnd=' + a[0] + ' ' + a[2].toInt32() + ',' + a[3].toInt32() + ' ' + a[4].toInt32() + 'x' + a[5].toInt32() + ' f=' + a[6],
  GetSystemMetrics: a => 'idx=' + a[0].toInt32(),
  CreateWindowExA: a => 'cls=' + s(a[1]) + ' title=' + s(a[2]) + ' style=' + a[3].toString(16) + ' ' + a[6].toInt32() + 'x' + a[7].toInt32(),
  ExitProcess: null, TerminateProcess: null, NtTerminateProcess: null,
};
for (const [name, fmt] of Object.entries(hooks)) {
  const p = Module.findGlobalExportByName(name);
  if (!p) { send('nohook ' + name); continue; }
  Interceptor.attach(p, { onEnter(args) {
    let msg;
    try { msg = fmt ? fmt(args) : ('a0=' + args[0] + ' a1=' + args[1] + '\n  ' + bt(this.context)); } catch (e) { msg = 'ERR ' + e; }
    if (QUIET.test(msg)) return;
    if (name === 'GetSystemMetrics') { if (seenOnce.has(msg)) return; seenOnce.add(msg); }
    send(name + ' ' + msg);
  } });
}
Process.setExceptionHandler(d => {
  send('EXCEPTION ' + d.type + ' at ' + DebugSymbol.fromAddress(d.address) + ' mem=' + JSON.stringify(d.memory || {}) + '\n  ' + bt(d.context));
  return false;
});
send('hooks installed');
