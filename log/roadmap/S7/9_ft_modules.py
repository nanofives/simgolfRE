"""S7.9 (I7's CMake finding): which freetype.dll a built simgolf_port.exe loads. Starts the app on the title menu (its own
PID only), reads its module list (EnumProcessModulesEx) until freetype.dll shows up or 20 s pass, prints every module whose
name holds `freetype` with its path, then terminates that PID. The PATH the app inherits is printed: on LIVING-PC it holds a
JDK's freetype.dll, which an app without its own copy loaded.
    ~/bin/py -3.12 log/roadmap/S7/9_ft_modules.py <simgolf_port.exe> <install dir> <scratch data dir>"""
import ctypes
import ctypes.wintypes as W
import os
import shutil
import subprocess
import sys
import time

exe, install, data = sys.argv[1:4]
shutil.rmtree(data, ignore_errors=True)
os.makedirs(data, exist_ok=True)
k32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)
psapi.EnumProcessModulesEx.argtypes = [W.HANDLE, ctypes.POINTER(W.HMODULE), W.DWORD, ctypes.POINTER(W.DWORD), W.DWORD]
psapi.GetModuleFileNameExW.argtypes = [W.HANDLE, W.HMODULE, W.LPWSTR, W.DWORD]
k32.OpenProcess.restype = W.HANDLE

path_ft = next((os.path.join(d, "freetype.dll") for d in os.environ.get("PATH", "").split(os.pathsep)
                if d and os.path.isfile(os.path.join(d, "freetype.dll"))), None)
print("PATH's freetype.dll:", path_ft)
print("beside the exe:", os.path.exists(os.path.join(os.path.dirname(exe), "freetype.dll")))
p = subprocess.Popen([exe, "--install", install, "--data", data, "--no-activate"])
found = []
try:
    t0 = time.time()
    while time.time() - t0 < 20 and p.poll() is None and not found:
        h = k32.OpenProcess(0x0410, False, p.pid)
        if h:
            mods = (W.HMODULE * 1024)()
            need = W.DWORD()
            if psapi.EnumProcessModulesEx(h, mods, ctypes.sizeof(mods), ctypes.byref(need), 3):
                for i in range(need.value // ctypes.sizeof(W.HMODULE)):
                    buf = ctypes.create_unicode_buffer(1024)
                    psapi.GetModuleFileNameExW(h, mods[i], buf, 1024)
                    if "freetype" in buf.value.lower():
                        found.append(buf.value)
            k32.CloseHandle(h)
        time.sleep(0.25)
finally:
    if p.poll() is None:
        p.terminate()
        p.wait(10)
print("app exit", p.returncode)
for f in found:
    print("loaded:", f)
if not found:
    print("loaded: (no freetype module seen)")
