One JS file per C3 batch, loaded by `re/frida/diff_hook.py` after `diff_fixtures.js` and before `diff_hook.js`:

    Object.assign(globalThis.DIFF_FIXTURES, {
      my_fixture() { /* seed memory */ return { obj: ptr('0x...') }; },
    });

Fixtures may call the base ones (`globalThis.DIFF_FIXTURES.golf_tables()`). Prefix fixture names with the batch id.
