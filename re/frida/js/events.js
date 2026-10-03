// Event tooling for scenarios.
//  watch(addr):            record [arg0, arg1, return value] of every call to a cdecl function (last 32).
//  inject(addr, a0, a1):   call a cdecl void f(int,int) ON THE GAME THREAD (queued, executed from the main
//                          loop's GetKeyboardState call, ~13/s) so game state is never touched concurrently.
//  readS16(addr):          read a signed 16-bit value from game memory.
//  stories():              golfer slots with a story (short +0xb0 != -1): [slot, story, partner +0xa2,
//                          stage +0xb2, counter +0xb4, hole +0x21]. Records: base 0x5794b8, stride 0x100.
// Injection is NOT organic gameplay: scenarios that use it must say so (re/tools/scenario.py).
const calls = {};
const queue = [];
const done = [];

rpc.exports = {
  watch(addr) {
    const key = addr.toString(16);
    calls[key] = [];
    Interceptor.attach(ptr(addr), {
      onEnter() {
        const sp = this.context.esp;
        this.rec = [sp.add(4).readS32(), sp.add(8).readS32(), null];
        calls[key].push(this.rec);
        if (calls[key].length > 32) calls[key].shift();
      },
      onLeave(rv) { this.rec[2] = rv.toInt32(); },
    });
  },
  calls(addr) { return calls[addr.toString(16)] || []; },
  inject(addr, a0, a1) { queue.push([addr, a0, a1]); return queue.length; },
  done() { return done.slice(); },
  readS16(addr) { return ptr(addr).readS16(); },
  stories() {
    const out = [];
    for (let i = 0; i < 0x98; i++) {   // 0x98 slots: loop bound in FUN_004289e0 (`if (0x97 < local_6c)`)
      const g = ptr(0x5794b8).add(i * 0x100);
      const story = g.add(0xb0).readS16();
      if (story !== -1)
        out.push([i, story, g.add(0xa2).readS16(), g.add(0xb2).readS16(), g.add(0xb4).readS16(), g.add(0x21).readS8()]);
    }
    return out;
  },
};

Interceptor.attach(Module.getGlobalExportByName('GetKeyboardState'), {
  onLeave() {
    while (queue.length) {
      const [addr, a0, a1] = queue.shift();
      // exceptions: 'propagate' so the shim's INT3 coverage VEH sees breakpoints inside the injected call
      // (by default Frida catches native exceptions here and the census never records the function).
      const f = new NativeFunction(ptr(addr), 'void', ['int', 'int'], { abi: 'mscdecl', exceptions: 'propagate' });
      try { f(a0, a1); done.push([addr, a0, a1, 'ok']); }
      catch (e) { done.push([addr, a0, a1, 'error ' + e]); }
    }
  },
});
