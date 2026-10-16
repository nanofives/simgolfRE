// c3fy fixtures fragment (2026-10-16): mainLoop 0x0040f5c0 has no path-1 fixture (it is called once per process and is
// verified by the whole-game scenario A/B, re/frida/scenario_ab.py). Nothing to register.
(function () {
  if (!globalThis.DIFF_FIXTURES) globalThis.DIFF_FIXTURES = {};
})();
