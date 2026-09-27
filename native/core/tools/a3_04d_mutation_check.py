"""Alpha 3 A3-04D mutation driver: each mutant must turn a3_04d_sd_log or
a3_04d_sd_log_runtime RED (ALPHA3_AUDIO.md section 21).

    python tools/a3_04d_mutation_check.py <build-dir> [M1,F2,...]

Each mutation edits one production file (core, runtime, or the device-only
sources the S-checks scan), rebuilds the two targets, runs them, restores the
file byte for byte and touches it (a restored file is older than the mutated
object, so ninja would otherwise keep the mutant). A mutant that does not
build counts as INVALID, not killed; a crash is a failing run (killed).
"""
import os
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOLS = "C:/Dev/TamaPoke/.build-tools/w64devkit/bin"
NINJA = TOOLS + "/ninja.exe"
RES_PACK = str(ROOT.parent / "assets" / "openu5-alpha1-resources.bin")
TARGETS = ["a3_04d_sd_log", "a3_04d_sd_log_runtime"]

LOG = "src/sd_diag_log.cpp"
LOG_H = "include/openu5/sd_diag_log.h"
REPORT = "src/perf_report.cpp"
MENU = "src/ui_debug_menu.cpp"
RUNTIME = "../targets/tdeck/main/alpha_runtime.cpp"
BOARD = "../targets/tdeck/main/tdeck_board.cpp"
MAIN = "../targets/tdeck/main/main.cpp"
SDLOG = "../targets/tdeck/main/sd_diagnostic_logger.cpp"
SMOKE = "../targets/tdeck/main/device_smoke_tests.cpp"

MUTATIONS = [
    # ---- the policy and the core log (the device's own writer logic)
    ("M1", "the log is ON at boot (the A3-04C behaviour)", LOG_H,
     "constexpr bool kSdDiagLoggingDefault = false;",
     "constexpr bool kSdDiagLoggingDefault = true;"),
    ("M2", "capture ignores the switch: every log call is queued for the card", LOG_H,
     "bool capturing() const { return enabled() && available_.load() && !failed(); }",
     "bool capturing() const { return available_.load() && !failed(); }"),
    ("M3", "serial is skipped while the card copy is off", LOG,
     "const int serial_result = serial ? serial(format, serial_arguments) : 0;",
     "const int serial_result = serial && capturing() ? serial(format, serial_arguments) : 0;"),
    ("M4", "a wake while off and closed still opens the log", LOG,
     "    if (!on && !open_) return w; // off and closed: the card is not touched\n",
     ""),
    ("M5", "switching off never closes the log (the writer keeps flushing)", LOG,
     "    if (!on) { // switched off: what was captured while on is written, then the file is closed\n"
     "        file_.close();\n        open_ = false;\n",
     "    if (!on) { // switched off: what was captured while on is written, then the file is closed\n"),
    ("M6", "switching off drops what was captured while on", LOG,
     "    if (line) { // the rest of the queue",
     "    if (line && on) { // the rest of the queue"),
    ("M7", "the writer is never idle: it wakes every 250 ms while off", LOG_H,
     "bool idle() const { return failed() || (!enabled() && !open_); }",
     "bool idle() const { return failed(); }"),
    ("M8", "switching on is not refused without a card / writer", LOG,
     "    if (on && (!available_.load() || failed())) return false;\n",
     ""),
    ("M9", "a card error leaves the switch on", LOG,
     "    failed_.store(true);\n    enabled_.store(false);\n",
     "    failed_.store(true);\n"),
    ("M10", "the A3-04C flush cadence changes: a flush on every wake", LOG,
     "now_ms - last_flush_ms_ >= kSdDiagFlushPeriodMs",
     "now_ms - last_flush_ms_ >= 1"),
    ("M11", "the A3-04C line prefix changes", LOG,
     '"[%010llu] "',
     '"[%llu] "'),
    # ---- the report and the line
    ("F1", "the scenario label drops the log's state", REPORT,
     "unsigned(sc.sfx_volume), sc.synth_bypass ? \" BYPASS\" : \"\", sd_log_suffix(sc.sd_log));",
     "unsigned(sc.sfx_volume), sc.synth_bypass ? \" BYPASS\" : \"\", \"\");"),
    ("F2", "the report's SD-log line does not say OFF", REPORT,
     "        else if (sd->off) // A3-04D",
     "        else if (false) // A3-04D"),
    ("F3", "the A3C_PERF line's sd= field does not say OFF", REPORT,
     "sd->off ? \"OFF:\" : \"\"",
     "\"\""),
    ("F4", "slow TFT transactions during SD bursts are not added up", REPORT,
     "    slow_sd_ += t.slow_xfers_sd;\n",
     ""),
    ("F5", "the longest SD-burst transaction is not kept (the last frame's instead)", REPORT,
     "if (t.xfer_max_sd_cycles > xfer_max_sd_cycles_) xfer_max_sd_cycles_ = t.xfer_max_sd_cycles;",
     "xfer_max_sd_cycles_ = t.xfer_max_sd_cycles;"),
    ("F6", "with no logger reporting, the A3-04C label changes", REPORT,
     "    case SdLogState::NotReported: break;\n    }\n    return \"\";",
     "    case SdLogState::NotReported: break;\n    }\n    return \" sdlog n/a\";"),
    # ---- the Developer row
    ("U1", "the SD row toggles the synth bypass", MENU,
     "else if(cursor_==g+1){if(diagnostics_.sd_log)diagnostics_.sd_log(diagnostics_.context,true);}",
     "else if(cursor_==g+1){if(diagnostics_.music_bypass)diagnostics_.music_bypass(diagnostics_.context,true);}"),
    ("U2", "the row says ON while the log is off", MENU,
     "st==SdLogState::On?\"ON\":st==SdLogState::Off?\"off\":\"n/a\"",
     "st==SdLogState::Off?\"ON\":st==SdLogState::On?\"off\":\"n/a\""),
    # ---- the runtime
    ("R1", "the scenario is not labelled with the log's state", RUNTIME,
     "    s.sd_log=sd_log_state(); // A3-04D\n",
     ""),
    ("R2", "the Developer row never reaches the device switch", RUNTIME,
     "services.sd_log=sd_log_probe;",
     ""),
    ("R3", "the probe switches the device the wrong way", RUNTIME,
     "!r.sd_log_perf_.set_enabled(on)){",
     "!r.sd_log_perf_.set_enabled(!on)){"),
    # ---- the device wiring (source scans)
    ("D1", "the writer never sleeps: it polls every 250 ms while off", SDLOG,
     "        if (g_log.idle()) {",
     "        if (false && g_log.idle()) {"),
    ("D2", "the log hook bypasses the core log", SDLOG,
     "    return g_log.mirror(g_serial_writer, &clock_ms, format, arguments);",
     "    return g_serial_writer(format, arguments);"),
    ("D3", "boot switches the log on", SDLOG,
     "    g_log.set_available(true);\n    return true;",
     "    g_log.set_available(true);\n    g_log.set_enabled(true);\n    return true;"),
    ("D4", "switching on does not wake the sleeping writer", SDLOG,
     "    if (on && g_writer) xTaskNotifyGive(g_writer);\n",
     ""),
    ("D5", "boot opens the log again (A3-04C's initialize_storage)", SDLOG,
     "    g_storage_ready.store(true, std::memory_order_release);\n    return true;\n}",
     "    g_storage_ready.store(true, std::memory_order_release);\n    size_t bytes = 0;\n"
     "    return open_active_log(\"ab\", bytes);\n}"),
    ("D6", "the burst flag is never raised", SDLOG,
     "        g_burst_active = 1;\n",
     ""),
    ("D7", "the Board never attributes a slow transaction to the SD log", BOARD,
     "        if (sd0 || sd1) {",
     "        if (false) {"),
    ("D8", "main.cpp never hands the Board the burst flag", MAIN,
     "                board.set_sd_activity_flag(tdeck::sdlog::burst_flag());\n",
     ""),
    # ---- unrelated card use must not depend on the log (S9)
    ("D9", "a save's storage transaction depends on the log's switch", SDLOG,
     "    return g_storage_mutex&&xSemaphoreTake(g_storage_mutex,portMAX_DELAY)==pdTRUE;",
     "    return g_log.enabled()&&g_storage_mutex&&xSemaphoreTake(g_storage_mutex,portMAX_DELAY)==pdTRUE;"),
    ("D10", "the smoke tests rely on the logger to create /sd/ultima5/logs", SMOKE,
     'mkdir("/sd/ultima5",0777);mkdir("/sd/ultima5/logs",0777);',
     'mkdir("/sd/ultima5",0777);'),
]


def run(cmd, cwd, env, timeout=900):
    return subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True, timeout=timeout)


def main():
    build = pathlib.Path(sys.argv[1]).resolve()
    only = set(sys.argv[2].split(",")) if len(sys.argv) > 2 else None
    env = dict(os.environ)
    env["PATH"] = TOOLS.replace("/", "\\") + os.pathsep + env.get("PATH", "")
    results = []
    for mid, what, rel, old, new in MUTATIONS:
        if only and mid not in only:
            continue
        path = (ROOT / rel).resolve()
        original = path.read_bytes()
        text = original.decode("utf-8")
        if "\r\n" in text:  # the checkout is autocrlf: anchors follow the file
            old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
        if text.count(old) != 1:
            results.append((mid, "NOT-APPLIED", what, f"anchor count {text.count(old)}"))
            print(mid, "NOT-APPLIED", what, flush=True)
            continue
        try:
            path.write_bytes(text.replace(old, new, 1).encode("utf-8"))
            b = run([NINJA, "-C", str(build)] + TARGETS, ROOT, env)
            if b.returncode != 0:
                verdict, detail = "INVALID", b.stdout[-400:]
            else:
                reds = []
                t = run([str(build / "a3_04d_sd_log.exe"), str(ROOT)], build, env, 600)
                red = [l.split(" ")[1] for l in t.stdout.splitlines() if l.startswith("RED ")]
                if t.returncode != 0:
                    reds.append("sd_log: " + (", ".join(red) if red else f"exit {t.returncode}"))
                r = run([str(build / "a3_04d_sd_log_runtime.exe"), RES_PACK], build, env, 600)
                red = [l.split(" ")[1] for l in r.stdout.splitlines() if l.startswith("RED ")]
                if r.returncode != 0:
                    reds.append("runtime: " + (", ".join(red) if red else f"exit {r.returncode}"))
                verdict = "KILLED" if reds else "SURVIVED"
                detail = " | ".join(reds)
        finally:
            path.write_bytes(original)
            now = time.time()
            os.utime(path, (now, now))
        results.append((mid, verdict, what, detail))
        print(mid, verdict, what, "::", detail, flush=True)
    # restored build, so the tree's objects match the sources again
    run([NINJA, "-C", str(build)] + TARGETS, ROOT, env)
    killed = sum(1 for r in results if r[1] == "KILLED")
    print(f"\n{killed}/{len(results)} killed; "
          f"{sum(1 for r in results if r[1] == 'SURVIVED')} survived; "
          f"{sum(1 for r in results if r[1] in ('INVALID', 'NOT-APPLIED'))} invalid/not applied")
    return 0 if killed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
