// Log every file opened for writing (CreateFileA/W with GENERIC_WRITE) and registry writes.
for (const [n, wide] of [['CreateFileA', false], ['CreateFileW', true]]) {
  Interceptor.attach(Module.getGlobalExportByName(n), { onEnter(a) {
    const access = a[1].toUInt32();
    if (access & 0x40000000) {
      let s = '?'; try { s = wide ? a[0].readUtf16String() : a[0].readAnsiString(); } catch (e) {}
      send('WRITE ' + s + ' disp=' + a[4].toUInt32());
    }
  }});
}
for (const n of ['RegSetValueExA', 'RegSetValueExW', 'RegCreateKeyExA', 'RegCreateKeyExW']) {
  const p = Module.findGlobalExportByName(n);
  if (p) Interceptor.attach(p, { onEnter(a) { send(n); } });
}
