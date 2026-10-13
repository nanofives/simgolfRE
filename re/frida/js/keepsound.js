// Keep sound.dll mapped on a machine with no audio output (c3ex, round 32, 2026-10-13; loaded by re/tools/game.py
// before resume when waveOutGetNumDevs() is 0 or SIMGOLF_KEEP_SOUND=1).
// With no active render endpoint initSound 0x00483e90 gets 3 from the music device open (resourceLoad 0x00487c00 on
// g_musicDevice 0x0083ad80) and calls freeSoundLib 0x00484060, which FreeLibrary's sound.dll (0x0048407a) about 1.3 s
// after the window: the module, and every sound.dll SG_HOOK with it, is gone before the first A/B key. This pins
// sound.dll (GetModuleHandleExA with GET_MODULE_HANDLE_EX_FLAG_PIN = 1) as soon as the game's LoadLibraryA returns it,
// so the FreeLibrary leaves it mapped. The game still runs its no-sound path (g_soundReady 0x0083afc4 stays 0,
// clearSoundPtrs 0x00484130 empties its table): sound.dll code runs only when a fixture calls it.
(function () {
  const k32 = Process.getModuleByName('kernel32.dll');
  const gmhx = new NativeFunction(k32.getExportByName('GetModuleHandleExA'), 'int', ['uint32', 'pointer', 'pointer'], 'stdcall');
  let pinned = false;
  Interceptor.attach(k32.getExportByName('LoadLibraryA'), {
    onEnter(a) { this.n = a[0].isNull() ? '' : a[0].readAnsiString(); },
    onLeave(r) {
      if (pinned || r.isNull() || !/(^|\\)sound\.dll$/i.test(this.n)) return;
      const out = Memory.alloc(Process.pointerSize);
      pinned = gmhx(1, Memory.allocAnsiString('sound.dll'), out) !== 0;
      send({ keepsound: pinned ? 'pinned' : 'pin failed', base: r.toString() });
    }
  });
})();
