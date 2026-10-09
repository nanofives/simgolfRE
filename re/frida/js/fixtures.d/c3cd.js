// Fixtures for batch c3cd (jgld.dll non-render): the window-message event sink, the command handler
// global, and the objects the four scalar deleting destructors run over.
// Reimplementations: shim/src/re/c3cd.cpp. Registry: re/frida/registry.d/c3cd.py.
//
// jgld.dll is relocated at run time (the probe log/c3/alt/c3cd_probe.py saw two different bases in two
// boots), so every module address below is built from Process.getModuleByName('jgld.dll').base; a
// hard-coded 0x10xxxxxx would read another module.
//
// Two of the three fixtures swap a MODULE GLOBAL the running game owns:
//   0x1012872c   the window-message event sink pointer (read twice per call by every on* handler and
//                by windowProc 0x100692b0; written only by Display::createWindow 0x100657e0)
//   0x10128728   the command handler (tested and called by onCommand 0x1006a0e0)
// Both are read only from the thread that owns the window, which is the process's first thread, so each
// of those fixtures SUSPENDS that thread before the swap and writes the real value back and resumes it
// when the key ends (c3av's pattern: the resume runs from setTimeout(0), i.e. after diff_hook's run()
// has returned all of the key's vectors). Everything the vectors need is built before the suspend; with
// the thread down the vectors only call jgld code over the fixture's own memory - no allocation, no
// lock, no Win32 call. If the suspend cannot be taken the fixture throws, so the A/B is recorded as
// ERROR instead of running against the game's live sink.

(function () {
  // Suspend the first thread and schedule the undo. `undo` is run after the resume handle is taken but
  // before the thread runs again... no: the thread is resumed last, so the global is already back.
  function freezeFirstThread(undo) {
    const k32 = Process.getModuleByName('kernel32.dll');
    const openThread = new NativeFunction(k32.getExportByName('OpenThread'), 'pointer',
                                          ['uint32', 'int', 'uint32'], 'stdcall');
    const suspend = new NativeFunction(k32.getExportByName('SuspendThread'), 'uint32', ['pointer'], 'stdcall');
    const resume = new NativeFunction(k32.getExportByName('ResumeThread'), 'uint32', ['pointer'], 'stdcall');
    const closeHandle = new NativeFunction(k32.getExportByName('CloseHandle'), 'int', ['pointer'], 'stdcall');
    // Toolhelp enumerates threads in creation order, so [0] is the process's first thread: the one that
    // called CreateWindow and therefore the only one that dispatches window messages.
    const threads = Process.enumerateThreads();
    const tid = threads.length ? threads[0].id : 0;
    if (!tid || tid === Process.getCurrentThreadId()) throw new Error('c3cd: no first thread to suspend');
    const h = openThread(0x0002 /* THREAD_SUSPEND_RESUME */, 0, tid);
    if (h.isNull()) throw new Error('c3cd: OpenThread failed');
    const prev = suspend(h);
    if (prev === 0xffffffff) { closeHandle(h); throw new Error('c3cd: SuspendThread failed'); }
    if (prev !== 0) { resume(h); closeHandle(h); throw new Error('c3cd: thread already suspended (' + prev + ')'); }
    globalThis.__c3cdFreeze = { tid: tid, resumed: -1 };
    setTimeout(function () {
      undo();
      globalThis.__c3cdFreeze.resumed = resume(h);
      closeHandle(h);
    }, 0);
  }

  const fill = (p, n, seed) => { for (let i = 0; i < n; i += 4) p.add(i).writeU32(((seed << 16) + i) >>> 0); };

  // __thiscall recording stub for one event-sink vtable slot: writes `tag` and its `n` dword arguments
  // into the sink's record area and returns its first stack argument. The sink carries the record cursor
  // (a byte offset from the object) at +4.
  //   8B 41 04            mov eax, [ecx+4]          ; cursor
  //   03 C1               add eax, ecx              ; eax = sink + cursor
  //   C7 00 <tag>         mov dword [eax], tag
  //   per argument i:  8B 54 24 (4i)  mov edx,[esp+4i]   /  89 50 (4i)  mov [eax+4i], edx
  //   8B 51 04            mov edx, [ecx+4]
  //   83 C2 (4(n+1))      add edx, 4*(n+1)
  //   89 51 04            mov [ecx+4], edx
  //   8B 44 24 04         mov eax, [esp+4]          ; return the first stack argument
  //   C2 (4n) 00          ret 4n                    ; __thiscall: the callee pops its stack arguments,
  //                                                 ;   which is what the caller's `cmp esi,esp` +
  //                                                 ;   __chkesp 0x1007e780 checks
  function sinkStub(tag, n) {
    const b = [0x8B, 0x41, 0x04, 0x03, 0xC1,
               0xC7, 0x00, tag & 0xff, (tag >>> 8) & 0xff, (tag >>> 16) & 0xff, (tag >>> 24) & 0xff];
    for (let i = 1; i <= n; i++) b.push(0x8B, 0x54, 0x24, 4 * i, 0x89, 0x50, 4 * i);
    b.push(0x8B, 0x51, 0x04, 0x83, 0xC2, 4 * (n + 1), 0x89, 0x51, 0x04);
    b.push(0x8B, 0x44, 0x24, 0x04);
    b.push(0xC2, 4 * n, 0x00);
    return b;
  }

  Object.assign(globalThis.DIFF_FIXTURES, {
    // ------------------------------------------------------------------ the nine on* forwarders
    //
    // A sink of our own at 0x1012872c for the length of the key. Its vtable holds a recording stub for
    // each of the nine slots the batch's handlers call (+0x00, +0x08, +0x10, +0x14, +0x18, +0x1c, +0x20,
    // +0x24, +0x28), each with that slot's own tag and argument count, and a `xor eax,eax; ret` in every
    // other slot. The whole 0x100-byte sink is the compared state, so a stub that wrote one dword too
    // many, or a handler that called the wrong slot or passed the arguments in the wrong order, changes
    // the hash.
    c3cd_sink() {
      const out = {};
      const page = Process.pageSize;
      const code = Memory.alloc(page);
      Memory.protect(code, page, 'rwx');

      const sink = Memory.alloc(0x100);
      fill(sink, 0x100, 0x5c0d);

      const vt = Memory.alloc(0x80);
      // every slot a 0-argument `xor eax,eax; ret`, so a slot this batch does not use is still callable
      const nullStub = code;
      code.writeByteArray([0x33, 0xC0, 0xC3]);
      for (let i = 0; i < 0x20; i++) vt.add(i * 4).writePointer(nullStub);

      // [slot offset, tag, stack argument count]
      const spec = [[0x00, 0xA0000000, 2],    // onWmSize        0x10069de0
                    [0x08, 0xA1000000, 2],    // onWmNcHitTest   0x10069e40
                    [0x10, 0xA2000000, 1],    // onChar          0x10069d7c
                    [0x14, 0xA3000000, 2],    // onWmMouseMove   0x10069f10
                    [0x18, 0xA4000000, 3],    // onWmMouseWheel  0x10069ea3
                    [0x1c, 0xA5000000, 3],    // onWmLButtonDown 0x10069f73
                    [0x20, 0xA6000000, 2],    // onWmLButtonUp   0x10069fe0
                    [0x24, 0xA7000000, 3],    // onWmRButtonDown 0x1006a043
                    [0x28, 0xA8000000, 2]];   // onWmRButtonUp   0x1006a0b0
      let off = 0x20;
      for (const [slot, tag, n] of spec) {
        const p = code.add(off);
        p.writeByteArray(sinkStub(tag, n));
        vt.add(slot).writePointer(p);
        off += 0x40;
      }
      sink.writePointer(vt);            // +0x00 vtable
      sink.add(4).writeU32(0x40);       // +0x04 record cursor (byte offset from the object)
      out.sink = sink;
      out.obj = sink;

      const gSink = Process.getModuleByName('jgld.dll').base.add(0x12872c);
      const saved = gSink.readPointer();
      freezeFirstThread(function () { gSink.writePointer(saved); });
      gSink.writePointer(sink);
      return out;
    },

    // --------------------------------------------- the command handler global, handler INSTALLED
    //
    // A __cdecl recording handler of ours in 0x10128728: it stores its two arguments in a buffer and
    // returns the first, so onCommand's return value is the handler's and varies per vector. The
    // handler does NOT pop its arguments (plain `ret`), which is what onCommand's `add esp,8` at
    // 0x1006a111 expects.
    //   B8 <buf>        mov eax, buf
    //   8B 54 24 04     mov edx, [esp+4]      ; first argument
    //   89 10           mov [eax], edx
    //   8B 54 24 08     mov edx, [esp+8]      ; second argument
    //   89 50 04        mov [eax+4], edx
    //   8B 44 24 04     mov eax, [esp+4]
    //   C3              ret
    c3cd_cmd_handler() {
      const out = {};
      const rec = Memory.alloc(0x40);
      fill(rec, 0x40, 0x5c1d);
      const page = Process.pageSize;
      const code = Memory.alloc(page);
      Memory.protect(code, page, 'rwx');
      code.writeByteArray([0xB8, 0, 0, 0, 0,
                           0x8B, 0x54, 0x24, 0x04, 0x89, 0x10,
                           0x8B, 0x54, 0x24, 0x08, 0x89, 0x50, 0x04,
                           0x8B, 0x44, 0x24, 0x04, 0xC3]);
      code.add(1).writePointer(rec);
      out.rec = rec;
      out.obj = rec;

      const gCmd = Process.getModuleByName('jgld.dll').base.add(0x128728);
      const saved = gCmd.readPointer();
      freezeFirstThread(function () { gCmd.writePointer(saved); });
      gCmd.writePointer(code);
      out.gCmd = gCmd;
      return out;
    },

    // ------------------------------------------------- the command handler global, handler NULL
    //
    // The same global forced to 0 for the length of the key, which is the only way to reach onCommand's
    // `je 0x1006a11d` side (the `xor eax,eax` return). The global itself is the compared state; no
    // vector changes it, so this key is branch coverage and not spread - the spread comes from
    // c3cd_cmd_handler.
    c3cd_cmd_null() {
      const out = {};
      const gCmd = Process.getModuleByName('jgld.dll').base.add(0x128728);
      const saved = gCmd.readPointer();
      freezeFirstThread(function () { gCmd.writePointer(saved); });
      gCmd.writePointer(ptr(0));
      out.gCmd = gCmd;
      out.obj = gCmd;
      return out;
    },

    // --------------------------------------- the global the two command-handler writers store into
    //
    // storeCommandHandler 0x10067f60 and Lib::setCommandHandler 0x10067f10 write their argument into
    // 0x10128728, so here the global IS the compared state. The twelve values the vectors store are
    // addresses of one-byte `ret` stubs of ours rather than invented numbers: the game's first thread is
    // suspended for the key, but a `ret` is harmless even if anything else reached the slot. The global
    // is set to 0 before the snapshot, so every vector's store shows as a change, and the real handler
    // is written back when the key ends.
    c3cd_cmd_store() {
      const out = {};
      const page = Process.pageSize;
      const code = Memory.alloc(page);
      Memory.protect(code, page, 'rwx');
      for (let i = 0; i < 12; i++) {
        code.add(i * 0x10).writeByteArray([0xC3]);     // ret
        out['h' + i] = code.add(i * 0x10);
      }
      const scratch = Memory.alloc(0x40);
      fill(scratch, 0x40, 0x5c2d);
      out.scratch = scratch;
      out.obj = scratch;

      const gCmd = Process.getModuleByName('jgld.dll').base.add(0x128728);
      const saved = gCmd.readPointer();
      freezeFirstThread(function () { gCmd.writePointer(saved); });
      gCmd.writePointer(ptr(0));
      out.gCmd = gCmd;
      return out;
    },

    // ----------------------------------------------- the four scalar deleting destructors' objects
    //
    // One block, carved into 24 slots per class at a larger stride than the object, so a store one field
    // too wide shows in the compared state. Nothing here is live jgld memory and no vector sets bit 0 of
    // the flags argument, so operator delete 0x1007e6d0 is never reached (log/c3/c3cd_notes.txt).
    //
    //   +0x0000  24 Matrix     slots, stride 0x60 (the object is 0x44: vptr + sixteen cells)
    //   +0x0900  24 Transform  slots, stride 0x40 (vptr, quaternion +4, position +0x14, flags +0x20)
    //   +0x0f00  24 LinkedList slots, stride 0x40 (vptr, m_data +4, m_flag +8, m_count +0xc,
    //                                              m_head +0x10, m_cur +0x14, m_ordered +0x18)
    //   +0x1500  24 ListNode   slots, stride 0x40 (vptr, prev +4, next +8, data +0xc, flag byte +0x10)
    //   +0x1b00  256 list nodes, stride 0x20, for the rings LinkedList::clear walks
    //   +0x3b00  the marker counter the node deleting-destructor stub bumps
    c3cd_objects() {
      const out = {};
      const blk = Memory.alloc(0x4000);
      fill(blk, 0x4000, 0x5c3d);
      out.blk = blk;
      out.obj = blk;

      const ctr = blk.add(0x3b00);
      ctr.writeU32(0);

      // __thiscall stub(int flag) in slot 0 of every fixture node's vtable: the scalar deleting
      // destructor LinkedList::clear 0x10006df0 calls with the argument 1 (0x10006e70, 0x10006ea8).
      // eax = ++*ctr; node[0x14] = eax; ret 4  (c3bw's stub, so the deletion ORDER is compared state).
      const page = Process.pageSize;
      const code = Memory.alloc(page);
      Memory.protect(code, page, 'rwx');
      code.writeByteArray([0xA1, 0, 0, 0, 0, 0x40, 0xA3, 0, 0, 0, 0, 0x89, 0x41, 0x14, 0xC2, 0x04, 0x00]);
      code.add(1).writePointer(ctr);
      code.add(7).writePointer(ctr);
      const nodeVt = Memory.alloc(0x20);
      for (let i = 0; i < 8; i++) nodeVt.add(i * 4).writePointer(code);

      for (let i = 0; i < 24; i++) {
        out['m' + i] = blk.add(i * 0x60);                 // Matrix
        out['x' + i] = blk.add(0x0900 + i * 0x40);        // Transform
        out['l' + i] = blk.add(0x0f00 + i * 0x40);        // LinkedList
        out['n' + i] = blk.add(0x1500 + i * 0x40);        // ListNode
      }

      // the rings: list i gets `len[i]` nodes, prev/next closed, each with the marker vtable
      let next = 0;
      const newNode = () => {
        const p = blk.add(0x1b00 + next * 0x20);
        next += 1;
        p.writePointer(nodeVt);
        p.add(0x0c).writeU32(0x7700 + next);              // data
        p.add(0x10).writeU8(next & 0xff);                 // flag byte
        return p;
      };
      const lens = [0, 1, 2, 3, 5, 1, 2, 4, 0, 3, 6, 1, 2, 0, 1, 2, 3, 4, 5, 6, 1, 2, 3, 0];
      for (let i = 0; i < 24; i++) {
        const list = out['l' + i];
        list.add(0x0c).writeU32(lens[i]);                 // m_count
        list.add(0x18).writeU32(0);                       // m_ordered
        if (lens[i] === 0) { list.add(0x10).writePointer(ptr(0)); list.add(0x14).writePointer(ptr(0)); continue; }
        const nodes = [];
        for (let k = 0; k < lens[i]; k++) nodes.push(newNode());
        for (let k = 0; k < nodes.length; k++) {
          nodes[k].add(4).writePointer(nodes[(k + nodes.length - 1) % nodes.length]);   // prev
          nodes[k].add(8).writePointer(nodes[(k + 1) % nodes.length]);                  // next
        }
        list.add(0x10).writePointer(nodes[0]);            // m_head
        list.add(0x14).writePointer(nodes[nodes.length - 1]);  // m_cur
      }
      return out;
    },
  });
})();
