"""Alpha 3 A3-04C mutation driver: each mutant must turn a3_04c_contention or
a3_04c_contention_runtime RED (ALPHA3_AUDIO.md section 20).

    python tools/a3_04c_mutation_check.py <build-dir> [K1,C2,...]

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
AUDIO_PACK = str(ROOT.parent / "assets" / "openu5-audio.bin")
RES_PACK = str(ROOT.parent / "assets" / "openu5-alpha1-resources.bin")
TARGETS = ["a3_04c_contention", "a3_04c_contention_runtime"]

STREAM = "src/audio_stream.cpp"
STREAM_H = "include/openu5/audio_stream.h"
REPORT = "src/perf_report.cpp"
REPORT_H = "include/openu5/perf_report.h"
MENU = "src/ui_debug_menu.cpp"
RUNTIME = "../targets/tdeck/main/alpha_runtime.cpp"
BOARD = "../targets/tdeck/main/tdeck_board.cpp"
AUDIO = "../targets/tdeck/main/tdeck_audio.cpp"
MAIN = "../targets/tdeck/main/main.cpp"
SDLOG = "../targets/tdeck/main/sd_diagnostic_logger.cpp"

MUTATIONS = [
    # ---- the probe on the pump
    ("K1", "the probe is ignored: the synth still renders under it", STREAM,
     "    if (music_.active() && !music_bypass_)\n",
     "    if (music_.active())\n"),
    ("K2", "the probe discards the synth's output instead of skipping it (the song advances)", STREAM,
     "    if (music_.active() && !music_bypass_)\n        music_.render(music_block_, kAudioBlockFrames, kSfxOutputRateHz, music_gain_q15);\n    else std::memset(music_block_, 0, sizeof(music_block_));",
     "    if (music_.active())\n        music_.render(music_block_, kAudioBlockFrames, kSfxOutputRateHz, music_gain_q15);\n    if (music_bypass_ || !music_.active()) std::memset(music_block_, 0, sizeof(music_block_));"),
    ("K3", "the probe counts as no work: the channel turns off", STREAM_H,
     "    bool has_work() const { return !sfx_.idle() || music_.active(); }",
     "    bool has_work() const { return !sfx_.idle() || (music_.active() && !music_bypass_); }"),
    ("K4", "the audio window never reports the probe", STREAM,
     "    out.music_bypass = music_bypass_;\n",
     ""),
    ("K5", "task busy = the whole delivery interval (the write's wait not subtracted)", STREAM,
     "const uint32_t busy = period_us > write_us ? period_us - write_us : 0;",
     "const uint32_t busy = period_us;"),
    # ---- the counters
    ("C1", "row classes swapped: the idle rows' fill is the busy rows' cycles", REPORT,
     "out.row_fill_idle_x10 = per_row_x10(fill_idle_cycles_, rows_idle_);",
     "out.row_fill_idle_x10 = per_row_x10(fill_busy_cycles_, rows_idle_);"),
    ("C2", "viewport frames are not kept apart from the rest", REPORT,
     "    if (t.viewport_full) {\n        ++viewport_frames_;",
     "    if (true) {\n        ++viewport_frames_;"),
    ("C3", "the fill share forgets the yields", REPORT,
     "fill_sum_ += tft_us > xfer_us + yield_us ? tft_us - xfer_us - yield_us : 0;",
     "fill_sum_ += tft_us > xfer_us ? tft_us - xfer_us : 0;"),
    ("C4", "an untimed frame is treated as timed (no cpu_mhz guard)", REPORT,
     "    if (!t.cpu_mhz) return;\n    ++timed_frames_;",
     "    ++timed_frames_;"),
    ("C5", "late yields are not accumulated", REPORT,
     "    late_yields_ += t.late_yields;\n",
     ""),
    # ---- the report and the line
    ("F1", "the report loses its contention section", REPORT,
     "    if (in.scenario || in.contention || in.sdlog) contention_section(out, in);",
     "    if (false && (in.scenario || in.contention || in.sdlog)) contention_section(out, in);"),
    ("F2", "no music capability: the BYPASS label is dropped (the bug found on the first run)", REPORT,
     "std::snprintf(out, cap, \"music n/a  sfx %u%%%s\", unsigned(sc.sfx_volume), sc.synth_bypass ? \" BYPASS\" : \"\");",
     "std::snprintf(out, cap, \"music n/a  sfx %u%%\", unsigned(sc.sfx_volume));"),
    ("F3", "the report buffer back at A3-04B's 48 lines", REPORT_H,
     "constexpr size_t kPerfReportMaxLines = 64;",
     "constexpr size_t kPerfReportMaxLines = 48;"),
    ("F4", "the line buffer too short for the worst realistic window", REPORT_H,
     "constexpr size_t kContentionLineBytes = 720;",
     "constexpr size_t kContentionLineBytes = 640;"),
    ("F5", "the line loses its row-level split", REPORT,
     "                     \" | rows=%lu busy=%lu%% fill=%s/%s xfer=%s/%s\",",
     "                     \" | rows=%lu busy=%lu%%%.0s%.0s%.0s%.0s\","),
    # ---- the runtime
    ("R1", "the frame's TFT timing is dropped on the way to the counters", RUNTIME,
     "contention_.on_frame(uint32_t(frame_t0-logic_t0),uint32_t(tft_t1-tft_t0),tft_timing);",
     "contention_.on_frame(uint32_t(frame_t0-logic_t0),uint32_t(tft_t1-tft_t0),openu5::TftTiming{});"),
    ("R2", "a live read does not start a new contention window", RUNTIME,
     "    contention_.reset(uint64_t(esp_timer_get_time()));\n    if(sd_log_perf_.reset)sd_log_perf_.reset();",
     "    if(sd_log_perf_.reset)sd_log_perf_.reset();"),
    ("R3", "the probe flips its state without telling the audio task", RUNTIME,
     "if(!r.audio_perf_||!r.audio_perf_->set_music_bypass(!r.music_bypass_)){",
     "if(!r.audio_perf_){"),
    ("R4", "the scenario never says BYPASS", RUNTIME,
     "    s.synth_bypass=music_bypass_;",
     "    s.synth_bypass=false;"),
    ("R5", "the SD-log bursts are never read", RUNTIME,
     "    in.scenario=&scenario;in.contention=&contention;in.sdlog=has_sd?&sd:nullptr;\n    if(with_guard)",
     "    in.scenario=&scenario;in.contention=&contention;in.sdlog=nullptr;(void)has_sd;\n    if(with_guard)"),
    # ---- the Developer row
    ("U1", "the probe row never shows its state", MENU,
     "kBypassItem,on?\"ON\":\"off\");",
     "kBypassItem,\"off\");(void)on;"),
    ("U2", "the probe row runs the live stats instead", MENU,
     "   else if(cursor_==g+1){if(diagnostics_.music_bypass)diagnostics_.music_bypass(diagnostics_.context,true);}",
     "   else if(cursor_==g+1){if(diagnostics_.audio_stats)diagnostics_.audio_stats(diagnostics_.context);}"),
    # ---- the device wiring (scanned)
    ("D1", "a viewport row is written without timing", BOARD,
     "        ESP_RETURN_ON_ERROR(tft_row(transaction, mark), kTag, \"write RGB565 row\");",
     "        ESP_RETURN_ON_ERROR(spi_device_transmit(display_handle(display_device_), &transaction), kTag, \"write RGB565 row\");(void)mark;"),
    ("D2", "a draw-loop yield is untimed", BOARD,
     "        if((++chunks&31)==0)tft_yield();",
     "        if((++chunks&31)==0)vTaskDelay(1);"),
    ("D3", "the audio flag stays 1 while the task waits for the DMA", AUDIO,
     "    owner_.active_ = 0; // A3-04C: blocked until the DMA frees a descriptor\n",
     ""),
    ("D4", "the probe never reaches the pump", AUDIO,
     "        pump_.set_music_bypass(bypass_requested_.load()); // A3-04C probe, between blocks\n",
     ""),
    ("D5", "main.cpp never gives the Board the audio flag", MAIN,
     "                board.set_audio_activity_flag(audio_backend.activity_flag());\n",
     ""),
    ("D6", "the SD-log bursts are never counted", SDLOG,
     "            g_perf_bursts.fetch_add(1);\n",
     ""),
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
                t = run([str(build / "a3_04c_contention.exe"), str(ROOT), AUDIO_PACK], build, env, 600)
                red = [l.split(" ")[1] for l in t.stdout.splitlines() if l.startswith("RED ")]
                if t.returncode != 0:
                    reds.append("contention: " + (", ".join(red) if red else f"exit {t.returncode}"))
                r = run([str(build / "a3_04c_contention_runtime.exe"), RES_PACK], build, env, 600)
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
