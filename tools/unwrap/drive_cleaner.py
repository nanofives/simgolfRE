"""Drive Safedisc2Cleaner (GUI) unattended: Unwrap -> pick golf.exe -> OK every prompt -> log status.
Must run elevated (the Cleaner's manifest is requireAdministrator, and UIPI blocks a non-elevated driver)."""
import time, pathlib, traceback
from pywinauto import Application, findwindows

W = pathlib.Path(__file__).parent
TARGET = W / "golf.exe"
log = open(W / "drive_cleaner.log", "w", buffering=1)


def top_windows(app):
    return [app.window(handle=h) for h in findwindows.find_windows(process=app.process)]


app = None
try:
    app = Application(backend="win32").start(str(W / "Safedisc2Cleaner_version.exe"), work_dir=str(W))
    time.sleep(2)
    main = app.window(title_re="Safedisc.*Cleaner.*", class_name="#32770")
    main_h = main.handle
    main = app.window(handle=main_h)
    main.child_window(control_id=1001).click()

    dlg = None
    for _ in range(30):
        for w in top_windows(app):
            if w.handle != main_h and w.class_name() == "#32770" and w.is_visible():
                dlg = w
        if dlg:
            break
        time.sleep(0.5)
    log.write("file dialog: %r\n" % dlg.window_text())
    edits = [c for c in dlg.descendants() if c.class_name() == "Edit"]
    edits[0].set_edit_text(str(TARGET))
    edits[0].type_keys("{ENTER}")

    t0, seen, done = time.time(), set(), False
    while time.time() - t0 < 300 and not done:
        status = main.child_window(control_id=1010).window_text()
        if status not in seen:
            seen.add(status)
            log.write("STATUS %r\n" % status)
        for w in top_windows(app):
            try:  # popups can close between enumeration and reading (race seen on the v1.00 run)
                if w.handle == main_h or w.class_name() != "#32770" or not w.is_visible():
                    continue
                texts = [c.window_text() for c in w.children()]
                log.write("POPUP %r %r\n" % (w.window_text(), texts))
                if any("created" in t for t in texts):
                    done = True
                btns = [c for c in w.children() if c.class_name() == "Button"]
                if btns:
                    btns[0].click()
            except Exception as e:
                log.write("popup vanished: %s\n" % type(e).__name__)
            time.sleep(1)
        time.sleep(0.5)
    log.write("done\n" if done else "timeout\n")
except Exception:
    log.write(traceback.format_exc())
finally:
    if app:
        try:
            app.kill()
        except Exception:
            pass
