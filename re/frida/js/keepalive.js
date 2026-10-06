// Loaded before the fixtures by diff_hook.py, which also rewrites `Memory.alloc(` to `__keepAlloc(` in the fixture
// sources (Memory.alloc itself is read-only in Frida). Frida frees a Memory.alloc block once no JS value references
// it; a fixture that links blocks only through native pointers (a parent stored at view+0x130, child arrays, list
// nodes) would see them freed mid-run and both arms would then read garbage (c3d View::toParent with a parent: RED,
// then an access violation, 2026-10-07). Every fixture block stays referenced here for the life of the script.
globalThis.__diffKeepAlive = [];
globalThis.__keepAlloc = function (size, options) {
  const p = options === undefined ? Memory.alloc(size) : Memory.alloc(size, options);
  globalThis.__diffKeepAlive.push(p);
  return p;
};
