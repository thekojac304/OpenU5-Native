"""One-shot isolated DOSBox-X frame attempt for Batch 49.

The save is diagnostically patched and cannot establish ordinary progression.
Only a visible original framebuffer, if captured, may serve as pose evidence.
"""
import ctypes
from ctypes import wintypes
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "original/u5/ultima5"
EVIDENCE = ROOT / "re/verified/batch49-original-run"
EVIDENCE.mkdir(parents=True, exist_ok=True)
TEMP_ROOT = Path(tempfile.gettempdir()).resolve()
RUN = Path(tempfile.mkdtemp(prefix="openu5-batch49-original-run-"))
assert RUN.resolve().is_relative_to(TEMP_ROOT) and RUN.name.startswith("openu5-batch49-original-run-")
for source in SOURCE.iterdir():
    if source.is_file() and source.suffix.lower() not in {".txt", ".conf"} and source.name.lower() != "dosbox-x.exe":
        shutil.copy2(source, RUN / source.name)
save = bytearray((RUN / "SAVED.GAM").read_bytes())
save[0x2d9] = 23
save[0x2db] = 45
save[0x2f0] = 9
save[0x2f1] = 9
(RUN / "SAVED.GAM").write_bytes(save)
args = [
    str(SOURCE / "dosbox-x.exe"), "-conf", str(SOURCE / "dosbox-x.conf"),
    "-fastlaunch", "-nomenu", "-time-limit", "55",
    "-set", f"dosbox captures={EVIDENCE}",
    "-c", f"MOUNT C {RUN}", "-c", "C:",
    "-c", "AUTOTYPE -w 12 -p 0.2 j",
    "-c", "ULTIMA5.COM",
]
print("synthetic diagnostic save: castle loc 17 floor 0, (9,9), 23:45")
print("command:", args)
with (EVIDENCE / "emulator-stdout.log").open("w", encoding="utf-8") as log:
    proc = subprocess.Popen(args, cwd=RUN, stdout=log, stderr=subprocess.STDOUT)
    time.sleep(27)
    user32 = ctypes.windll.user32
    handles = []
    EnumWindowsProc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def callback(hwnd, _):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value == proc.pid and user32.IsWindowVisible(hwnd):
            handles.append(hwnd)
        return True
    user32.EnumWindows(EnumWindowsProc(callback), 0)
    print("emulator pid:", proc.pid, "visible HWNDs:", handles)
    if handles:
        hwnd = handles[0]
        user32.ShowWindow(hwnd, 9)
        user32.SetForegroundWindow(hwnd)
        time.sleep(1)
        for vk, flags in ((0x7A, 0), (0x50, 0), (0x50, 2), (0x7A, 2)):
            user32.keybd_event(vk, 0, flags, 0)
            time.sleep(0.1)
    time.sleep(3)
    print("capture files:", [(p.name, p.stat().st_size) for p in EVIDENCE.iterdir()])
    proc.wait(timeout=35)
    print("emulator exit:", proc.returncode)
# The synthetic copy is disposable and remains wholly within the named temp root.
assert RUN.resolve().is_relative_to(TEMP_ROOT) and RUN.name.startswith("openu5-batch49-original-run-")
shutil.rmtree(RUN)
