# jgld_8: libpng's default error handler and the array teardown registrars (2026-10-11)

Named by round 28's c3ds. `png_default_error` 0x100792b0 is libpng 1.0.5's default error callback: png_error 0x10078fe0
calls it when the png_struct has no user handler; it prints the message to jgld's stderr stream with _fprintf and longjmps
back to the png_struct's jmp_buf (the path c3ds's longjmp catcher returns to the harness).

`registerArrayE0Teardown` 0x10065050, `registerModesTeardown` 0x10065160 and `registerDevmodesTeardown` 0x10065270 are the
second calls of the dynInit_* stubs named in jgld_7 (dynInit_arrayE0 0x10064fd0, dynInit_modes 0x100650e0,
dynInit_devmodes 0x100651f0): each pushes its global array's destructor stub and registers it with _atexit 0x1007f080.
All three match 100% (re/match/jgld_raw_10.cpp).
