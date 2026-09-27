"""Alpha 3 A3-04E mutation driver: each mutant must turn a3_04e_pacing or
a3_04e_pacing_runtime RED (ALPHA3_AUDIO.md section 22).

    python tools/a3_04e_mutation_check.py <build-dir> [K1,D3,...]

Each mutation edits one production file (the core policy/counters/report/menu,
the runtime, or the device-only sources -- tdeck_board.cpp is compiled into the
runtime test over host_tests/board_shims; main.cpp, tdeck_input.cpp,
tdeck_audio.cpp and alpha_save.cpp only by the S-scans), rebuilds the two
targets, runs them, restores the file byte for byte and touches it (a restored
file is older than the mutated object, so ninja would otherwise keep the
mutant). A mutant that does not build counts as INVALID, not killed; a crash is
a failing run (killed).
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
TARGETS = ["a3_04e_pacing", "a3_04e_pacing_runtime"]

POLICY = "include/openu5/render_pacing.h"
NAMES = "src/render_pacing.cpp"
REPORT = "src/perf_report.cpp"
MENU = "src/ui_debug_menu.cpp"
RUNTIME = "../targets/tdeck/main/alpha_runtime.cpp"
BOARD = "../targets/tdeck/main/tdeck_board.cpp"
MAIN = "../targets/tdeck/main/main.cpp"
INPUT = "../targets/tdeck/main/tdeck_input.cpp"
AUDIO = "../targets/tdeck/main/tdeck_audio.cpp"
SAVE = "../targets/tdeck/main/alpha_save.cpp"
IDLE = "../targets/tdeck/main/idle_service.cpp"

MUTATIONS = [
    # ---- the policy
    ("K1", "the yield mode still sleeps to the next tick (A3-04D's pause)", POLICY,
     "constexpr uint32_t tft_pause_ticks(TftPacing p) { return p == TftPacing::TickSleep ? 1u : 0u; }",
     "constexpr uint32_t tft_pause_ticks(TftPacing p) { return p == TftPacing::TickSleep ? 1u : 1u; }"),
    ("K2", "production draws with the legacy tick sleep", POLICY,
     "    TftPacing tft = TftPacing::Yield;",
     "    TftPacing tft = TftPacing::TickSleep;"),
    ("K3", "the cooperative cadence changes (every 32 rows)", POLICY,
     "constexpr int kTftRowsPerYield = 16;",
     "constexpr int kTftRowsPerYield = 32;"),
    ("K4", "the idle wait rounds to zero ticks (the A3-04D defect again)", POLICY,
     "constexpr uint32_t kLoopIdleWaitTicks = 1;",
     "constexpr uint32_t kLoopIdleWaitTicks = 0;"),
    ("K5", "the gate is ignored: the loop sleeps through paced scenes", POLICY,
     "    return p == LoopPacing::IdleWait && may_sleep ? kLoopIdleWaitTicks : 0u;",
     "    (void)may_sleep;\n    return p == LoopPacing::IdleWait ? kLoopIdleWaitTicks : 0u;"),
    ("K6", "the legacy spin sleeps too", POLICY,
     "    return p == LoopPacing::IdleWait && may_sleep ? kLoopIdleWaitTicks : 0u;",
     "    (void)p;\n    return may_sleep ? kLoopIdleWaitTicks : 0u;"),
    ("K7", "production spins (the A3-04D loop)", POLICY,
     "    LoopPacing loop = LoopPacing::IdleWait;",
     "    LoopPacing loop = LoopPacing::Spin;"),
    # ---- the counters, the report, the line
    ("C1", "the full-screen repaint is counted as a walking frame", REPORT,
     "    if (t.full_screen) {\n        ++full_frames_;",
     "    if (false) {\n        ++full_frames_;"),
    ("C2", "pauses per viewport frame are averaged over every frame", REPORT,
     "    if (t.viewport_full) {\n        ++timed_viewport_frames_;",
     "    if (true) {\n        ++timed_viewport_frames_;"),
    ("C3", "input wakes of the loop are not counted", REPORT,
     "    if (input) ++loop_input_wakes_;\n",
     "    (void)input;\n"),
    ("C4", "the longest idle wait is not kept (the last instead)", REPORT,
     "    if (wait_us > loop_wait_max_) loop_wait_max_ = wait_us;",
     "    loop_wait_max_ = wait_us;"),
    ("F1", "the names are swapped: A3-04E reads as the legacy pacing", NAMES,
     'return p == TftPacing::TickSleep ? "TICK" : "yield";',
     'return p == TftPacing::TickSleep ? "yield" : "TICK";'),
    ("F2", "the report always names A3-04E's pacing", REPORT,
     "tft_pacing_name(sc->pacing.tft), loop_pacing_name(sc->pacing.loop));",
     "tft_pacing_name(kPacingDefault.tft), loop_pacing_name(kPacingDefault.loop));"),
    ("F3", "the pacing leaks into A3C_PERF (old and new runs no longer diff)", REPORT,
     '    line.add("scen=[%s]", sc);',
     '    line.add("scen=[%s%s]", sc, in.scenario && in.scenario->pacing_reported ? " pace" : "");'),
    ("F4", "A3E_PACE loses the loop's waits", REPORT,
     '" | loop n=%lu /s=%lu max=%s wait=%lu:%s/%s in=%lu asleep=%s"',
     '" | loop n=%lu /s=%lu max=%s w=%lu:%s/%s in=%lu asleep=%s"'),
    # ---- the Developer rows
    ("U1", "the TFT row toggles the loop probe", MENU,
     "else if(cursor_==g+1){if(diagnostics_.legacy_tft_pacing)diagnostics_.legacy_tft_pacing(diagnostics_.context,true);}",
     "else if(cursor_==g+1){if(diagnostics_.legacy_loop_spin)diagnostics_.legacy_loop_spin(diagnostics_.context,true);}"),
    ("U2", "one row too few: the older rows move", MENU,
     "case UiDebugCategory::Diagnostics:return 8+debug_diagnostic_group_count();",
     "case UiDebugCategory::Diagnostics:return 7+debug_diagnostic_group_count();"),
    ("U3", "the TFT row reads off while the legacy pacing is on", MENU,
     '"%s: %s",kLegacyTftItem,on?"ON":"off"',
     '"%s: %s",kLegacyTftItem,on?"off":"ON"'),
    # ---- the runtime
    ("R1", "the Board is never handed the pacing", RUNTIME,
     "    board.set_tft_pacing(pacing_.tft); // A3-04E: the draw loops' pause (section 22)\n",
     ""),
    ("R2", "the gate forgets the combat enemy beat", RUNTIME,
     "    if(next_enemy_step_us_)return false;                     // combat: the enemy beat\n",
     ""),
    ("R3", "the gate forgets the narrative / Camp pacer", RUNTIME,
     "    if(narrative_pacer_.active()||narrative_pacer_.mounted())return false;\n",
     ""),
    ("R4", "the gate forgets the audio benchmark", RUNTIME,
     "    if(audio_bench_.running()||smoke_.view().running)return false;",
     "    if(smoke_.view().running)return false;"),
    ("R5", "the gate forgets a frame still owed", RUNTIME,
     "    if(dirty_||dungeon_presentation_pending_)return false;",
     "    if(dungeon_presentation_pending_)return false;"),
    ("R6", "the two probes are bound to each other's rows", RUNTIME,
     "services.legacy_tft_pacing=legacy_tft_probe;services.legacy_loop_spin=legacy_loop_probe;",
     "services.legacy_tft_pacing=legacy_loop_probe;services.legacy_loop_spin=legacy_tft_probe;"),
    ("R7", "the scenario does not report the pacing", RUNTIME,
     "    s.pacing_reported=true;s.pacing=pacing_; // A3-04E\n",
     ""),
    ("R8", "the heartbeat does not log A3E_PACE", RUNTIME,
     '    {char line[openu5::kPacingLineBytes];pacing_line(line,sizeof(line),++heartbeat_seq_);ESP_LOGI(kTag,"A3E_PACE %s",line);}\n',
     ""),
    # ---- the Board (compiled into the runtime test) and the device wiring
    ("D1", "Board::tft_yield is A3-04D's: always vTaskDelay(1)", BOARD,
     "    if (const uint32_t ticks = openu5::tft_pause_ticks(tft_pacing_)) vTaskDelay(ticks);\n"
     "    else if (!(idle_ && idle_->enforce())) taskYIELD();",
     "    vTaskDelay(1);"),
    ("D2", "the viewport loop no longer pauses", BOARD,
     'ESP_RETURN_ON_ERROR(tft_row(transaction, mark), kTag, "write RGB565 row");\n'
     "        if(openu5::tft_row_yield_due(row))tft_yield();",
     'ESP_RETURN_ON_ERROR(tft_row(transaction, mark), kTag, "write RGB565 row");'),
    ("D3", "a faster fill that skips half of every rectangle in the yield mode", BOARD,
     "        remaining -= count;",
     "        remaining -= count * (tft_pacing_ == openu5::TftPacing::Yield ? 2 : 1);"),
    ("D4", "the yield mode abandons the rest of the viewport at its first pause", BOARD,
     'ESP_RETURN_ON_ERROR(tft_row(transaction, mark), kTag, "write RGB565 row");\n'
     "        if(openu5::tft_row_yield_due(row))tft_yield();",
     'ESP_RETURN_ON_ERROR(tft_row(transaction, mark), kTag, "write RGB565 row");\n'
     "        if(openu5::tft_row_yield_due(row)){tft_yield();if(tft_pacing_==openu5::TftPacing::Yield)break;}"),
    ("D5", "leaving the Developer screen is not marked a full-screen repaint", BOARD,
     "    if(debug_drawn_){++tft_timing_.full_screen;",
     "    if(debug_drawn_){"),
    ("D6", "main.cpp spins again (pdMS_TO_TICKS(5) = 0 ticks)", MAIN,
     "        const uint32_t wait_ticks=ready?runtime.loop_wait_ticks():openu5::kLoopIdleWaitTicks;",
     "        const uint32_t wait_ticks=pdMS_TO_TICKS(5);"),
    ("D7", "the idle wait consumes the input event it woke for", INPUT,
     "    return xQueuePeek(event_queue_, &event, ticks) == pdTRUE;",
     "    return xQueueReceive(event_queue_, &event, ticks) == pdTRUE;"),
    ("D8", "the idle wait busy-waits a tick", MAIN,
     "            const bool woke=input.wait_for_event(wait_ticks);",
     "            bool woke=false;while(esp_timer_get_time()-wait_t0<10000){}"),
    ("D9", "the audio task moves to core 0 (a placement change A3-04E must not make)", AUDIO,
     'kTaskStackBytes, this, 3, &task_, 1)',
     'kTaskStackBytes, this, 3, &task_, 0)'),
    ("D10", "the save path starts to persist the pacing", SAVE,
     "bool AlphaSaveService::reserve_dma_headroom(){",
     "static const char *kPacingKey=\"pacing\";\nbool AlphaSaveService::reserve_dma_headroom(){"),
    # ---- A3-04E.1 (section 23): the idle-service guarantee
    ("I1", "the budget outlives the 5 s watchdog", POLICY,
     "constexpr uint32_t kIdleServiceBudgetUs = 200000;",
     "constexpr uint32_t kIdleServiceBudgetUs = 6000000;"),
    ("I2", "the guard never asks for a sleep", "src/render_pacing.cpp",
     "    return gap >= kIdleServiceBudgetUs;",
     "    return gap >= kIdleServiceBudgetUs && false;"),
    ("I3", "an idle pass is not recognised (the counter's movement ignored)", "src/render_pacing.cpp",
     "    if (count != last_count_) { // the idle loop ran",
     "    if (false) { // the idle loop ran"),
    ("I4", "the enforcement yields instead of sleeping", IDLE,
     "        vTaskDelay(1);\n        ++sleeps;",
     "        taskYIELD();\n        ++sleeps;"),
    ("I5", "no sleep at all per enforcement", POLICY,
     "constexpr uint32_t kIdleServiceMaxSleeps = 3;",
     "constexpr uint32_t kIdleServiceMaxSleeps = 0;"),
    ("I6", "the hook keeps the core from idling (returns false)", IDLE,
     "    return true; // the core may still wait for an interrupt (waiti) after the hooks",
     "    return false;"),
    ("I7", "the draw loops' yield skips the guard", BOARD,
     "    else if (!(idle_ && idle_->enforce())) taskYIELD();",
     "    else taskYIELD();"),
    ("I8", "a loop pass ends without the guard", MAIN,
     "        idle_service.enforce();\n    }",
     "    }"),
    ("I9", "the counting hook is registered on core 1", MAIN,
     "esp_register_freertos_idle_hook_for_cpu(&tdeck::IdleService::core0_hook,0)",
     "esp_register_freertos_idle_hook_for_cpu(&tdeck::IdleService::core0_hook,1)"),
    ("I10", "the report leaves the idle gap out", REPORT,
     "    if (const IdleServiceStats *i = in.idle)\n        out.add(\"idle0 gap max",
     "    if (const IdleServiceStats *i = in.idle; i && false)\n        out.add(\"idle0 gap max"),
    ("I11", "A3E_PACE loses its heartbeat number", REPORT,
     '    if (in.heartbeat) line.add("hb=%lu ", (unsigned long)in.heartbeat);',
     '    if (in.heartbeat && false) line.add("hb=%lu ", (unsigned long)in.heartbeat);'),
    ("I12", "the runtime never reports the idle-service window", RUNTIME,
     "    in.idle=idle_service_?&idle_service_->stats():nullptr;in.heartbeat=heartbeat; // A3-04E.1",
     "    in.heartbeat=heartbeat; // A3-04E.1"),
    ("I13", "every heartbeat carries the same number", RUNTIME,
     "pacing_line(line,sizeof(line),++heartbeat_seq_)",
     "pacing_line(line,sizeof(line),heartbeat_seq_+1)"),
    ("I14", "a live read does not start a new idle-service window", RUNTIME,
     "    if(idle_service_)idle_service_->reset_stats(); // A3-04E.1\n",
     ""),
]


def run(cmd, cwd, env, timeout=900):
    return subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True, timeout=timeout)


def reds_of(result, name):
    red = [l.split(" ")[1] for l in result.stdout.splitlines() if l.startswith("RED ")]
    if result.returncode != 0:
        return [name + ": " + (", ".join(red) if red else f"exit {result.returncode}")]
    return []


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
        if "\r\n" in text:  # a CRLF checkout: anchors follow the file
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
                reds = reds_of(run([str(build / "a3_04e_pacing.exe"), str(ROOT)], build, env, 600), "pacing")
                reds += reds_of(run([str(build / "a3_04e_pacing_runtime.exe"), RES_PACK], build, env, 900), "runtime")
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
