// S4.10g, U-1421: x87 control word at sound.dll's codecDecodeFrame / codecEncodeFrame entries, read natively.
// Probe stub (7 bytes): 8B 44 24 04 = mov eax,[esp+4]; D9 38 = fnstcw [eax]; C3 = ret. Called through a NativeFunction
// on the thread being measured, so it stores that thread's word. Also logs the caller, thread id and a fuzzy backtrace.
// Controls: the word on the main thread at attach time (0x027f expected), and at the LEAVE of Voice_mixAddMono16, which
// loads 0x0f7f and leaves it set (fldcw 0x19911).
(function () {
  const SITES = {
    0x3a980: 'codecDecodeFrame', 0x3b150: 'codecEncodeFrame', 0x30840: 'openMmioSound', 0x316c0: 'loadWaveResource',
    0x184a0: 'streamRenderBlock', 0x30710: 'Sound5c450::openMmio', 0x393d0: 'WaveInDevice_requeueBuffer', 0x2a970: 'soundThreadPump', 0x19770: 'Voice_mixAddMono16', 0x19b20: 'Voice_mixSetMono16',
  };
  const stub = Memory.alloc(Process.pageSize);
  stub.writeByteArray([0x8B, 0x44, 0x24, 0x04, 0xD9, 0x38, 0xC3]);
  Memory.protect(stub, Process.pageSize, 'rwx');
  const fnstcw = new NativeFunction(stub, 'void', ['pointer'], 'mscdecl');
  globalThis.__stub = stub;  // Frida frees an allocation no JS value references
  const buf = Memory.alloc(8); globalThis.__buf = buf;
  function cw() { buf.writeU16(0xffff); fnstcw(buf); return buf.readU16(); }
  const counts = {};
  const seenThreads = {};
  function loc(a) {
    const m = Process.findModuleByAddress(a);
    return m ? m.name + '+0x' + a.sub(m.base).toString(16) : a.toString();
  }
  send({ probe: 'control_main_at_load', tid: Process.getCurrentThreadId(), cw: cw() });
  function install(base) {
    for (const k of Object.keys(SITES)) {
      const rva = parseInt(k), name = SITES[k];
      Interceptor.attach(base.add(rva), {
        onEnter(args) {
          this.t0 = cw();
          const tid = this.threadId;
          counts[name] = (counts[name] || 0) + 1;
          const first = !seenThreads[name + tid];
          seenThreads[name + tid] = 1;
          if (counts[name] > 40 && !first && name !== 'codecDecodeFrame' && name !== 'codecEncodeFrame') return;
          let bt = [];
          try { bt = Thread.backtrace(this.context, Backtracer.FUZZY).slice(0, 6).map(loc); } catch (e) {}
          send({ probe: 'enter', fn: name, n: counts[name], tid: tid, cw: this.t0, ret: loc(this.returnAddress), bt: bt });
        },
        onLeave() {
          if (name === 'Voice_mixAddMono16' || name === 'Voice_mixSetMono16') {
            counts[name + '_leave'] = (counts[name + '_leave'] || 0) + 1;
            if (counts[name + '_leave'] <= 3) send({ probe: 'leave', fn: name, tid: this.threadId, cw_enter: this.t0, cw_leave: cw() });
          } else if (name === 'codecDecodeFrame' || name === 'codecEncodeFrame') {
            if (counts[name] <= 40) send({ probe: 'leave', fn: name, tid: this.threadId, cw_enter: this.t0, cw_leave: cw() });
          }
        }
      });
    }
    send({ probe: 'installed', base: base.toString() });
  }
  for (const [va, name] of [[0x485550, 'VoiceRx::fwdDev35'], [0x49b7b0, 'netPoll'], [0x485450, 'VoiceRx::start']]) {
    Interceptor.attach(ptr(va), { onEnter() { counts[name] = (counts[name] || 0) + 1; if (counts[name] <= 3) send({ probe: 'exe', fn: name, tid: this.threadId, cw: cw() }); } });
  }
  const m = Process.findModuleByName('sound.dll');
  if (m) install(m.base);
  else {
    let done = false;
    Interceptor.attach(Process.getModuleByName('kernel32.dll').getExportByName('LoadLibraryA'), {
      onEnter(a) { this.n = a[0].isNull() ? '' : a[0].readAnsiString(); },
      onLeave(r) {
        if (done || r.isNull() || !/(^|\\)sound\.dll$/i.test(this.n)) return;
        done = true; install(r);
      }
    });
  }
  rpc.exports = {
    counts() { return counts; },
    cw() { return cw(); },
  };
})();
