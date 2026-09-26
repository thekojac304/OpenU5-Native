"""Batch 53A source mutations for the terminal ending (UiMode::Ending).

Each case breaks ONE thing the post-victory contract depends on, rebuilds
batch53a_ending_terminal and batch53_release_blockers, runs both against the
shipped pack and records their RED checks. A case is KILLED when either test
exits non-zero. Production is restored byte-for-byte and touched afterwards so
ninja never reuses a mutated object (host-and-firmware-toolchain)."""
from pathlib import Path
import os
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[3]
core = root / "native/core"
build = core / (sys.argv[1] if len(sys.argv) > 1 else "build-batch53a")
bin_dir = Path("C:/Dev/TamaPoke/.build-tools/w64devkit/bin")
env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"])
pack = str(root / "native/assets/openu5-alpha1-resources.bin")
targets = ["batch53a_ending_terminal", "batch53_release_blockers"]

RT = "native/targets/tdeck/main/alpha_runtime.cpp"
UI = "native/core/src/ui_session.cpp"
QW = "native/core/src/quest_world.cpp"
COMBAT = "native/core/src/combat.cpp"

cases = [
    # The session never enters the Ending on GameWon (core hook removed).
    ("ui_gamewon_no_ending", UI,
     "        if(e.kind==GameEventKind::GameWon)enter_ending();\n",
     ""),
    # enter_ending() is a no-op: neither the core hook nor the device resync
    # can put the session in the Ending (omit the ending-mode transition).
    ("enter_ending_noop", UI,
     "void UiSession::enter_ending() {\n",
     "void UiSession::enter_ending() {\n    return;\n"),
    # Dungeon/Exploration restored too early: the per-input resync may leave.
    ("ending_not_sticky", UI,
     "    if (base_mode_ == UiMode::Ending) return;\n    base_mode_ = m;\n",
     "    base_mode_ = m;\n"),
    # Movement allowed during the terminal ending.
    ("ending_routes_movement", UI,
     "    if (mode_ == UiMode::Ending) return true;\n",
     "    if (mode_ == UiMode::Ending) return handle_dungeon(a);\n"),
    # The Ending swallows transcript paging too.
    ("ending_swallows_paging", UI,
     "bool UiSession::handle_input(const UiAction &a) {\n",
     "bool UiSession::handle_input(const UiAction &a) {\n    if (mode_ == UiMode::Ending) return true;\n"),
    # The device leaves the Ending on the first resync although still won.
    ("device_leaves_while_won", RT,
     "    else if(!won&&was)ui_->leave_ending(",
     "    else if(was)ui_->leave_ending("),
    # A loaded save of a won game resumes play (device never enters).
    ("device_never_enters", RT,
     "    if(won&&!was)ui_->enter_ending();\n",
     "    if(false)ui_->enter_ending();\n"),
    # A load does not leave the Ending (the old sticky session survives it).
    ("load_keeps_ending", RT,
     "    ending_announced_=false;synchronize_ending(\"load\");\n",
     "    ending_announced_=false;\n"),
    # game-won cleared as the Ending begins.
    ("ending_clears_game_won", RT,
     "        ESP_LOGI(kTag,\"ENDING_MODE active site=%s gameplay_input=swallowed save=refused\",site);}",
     "        openu5::set_quest_flag(game_.quest,openu5::QuestFlag::GameWon,false);}"),
    # The party relocated out of Doom when the ending begins (invented escape).
    ("ending_relocates_party", RT,
     "        ESP_LOGI(kTag,\"ENDING_MODE active site=%s gameplay_input=swallowed save=refused\",site);}",
     "        dungeon_.active=false;context_.dungeon=false;game_.position.map={17,1};game_.position.xy={10,10};}"),
    # The System line repeats on every input (per-key noise).
    ("ending_announce_every_input", RT,
     "if(ui_->ending_active()&&!ending_announced_){ending_announced_=true;",
     "if(ui_->ending_active()){"),
    # Alt+S saves the ended game.
    ("alt_s_saves_ended_game", RT,
     "    }else if(shortcut==DeviceShortcut::Save&&ui_->ending_active()){",
     "    }else if(shortcut==DeviceShortcut::Save&&false){"),
    # System Menu -> Save saves the ended game.
    ("menu_saves_ended_game", RT,
     "    if(intent.kind==openu5::SystemMenuIntentKind::Save&&ui_->ending_active()){ok=false;",
     "    if(intent.kind==openu5::SystemMenuIntentKind::Save&&false){ok=false;"),
    # The final report line is suppressed.
    ("suppress_final_report", QW,
     "message(report+\"\\nto Lord British at Origin Systems!\");",
     "(void)report;"),
    # The final arena is not torn down (Batch 53's RB-1 regression).
    ("combat_active_after_ending", COMBAT,
     "world.game.rng.seed(state.rng.get_seed());world.combat=false;world.combat_context=nullptr;state.initialized=false;\n"
     "        emit(GameEventKind::CombatEnded);absorption_endgame",
     "world.game.rng.seed(state.rng.get_seed());\n        emit(GameEventKind::CombatEnded);absorption_endgame"),
]


def build_and_run(log):
    result = subprocess.run([str(bin_dir / "cmake.exe"), "--build", str(build), "--target", *targets],
                            cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        log.write("\nBUILD FAILED\n")
        return None
    codes, red = [], []
    for t in targets:
        out = subprocess.run([str(build / f"{t}.exe"), pack], cwd=build, env=env,
                             capture_output=True, text=True, errors="replace", timeout=900)
        lines = [l for l in (out.stdout + out.stderr).splitlines() if not l.startswith("[")]
        red += [f"{t.split('_')[0]}:{l.split()[1]}" for l in lines if l.strip().startswith("RED ")]
        log.write(f"==== {t}\n")
        log.write("\n".join(l for l in lines if l.strip().startswith(("RED ", "GREEN ")) or "checks" in l))
        log.write(f"\n==== {t} exit={out.returncode}\n")
        codes.append(out.returncode)
    return max(codes), red


def touch(path):
    now = time.time()
    os.utime(path, (now, now))


originals = {}
results = []
try:
    for name, rel, old, new in cases:
        path = root / rel
        if rel not in originals:
            originals[rel] = path.read_bytes()
        raw = originals[rel].decode("utf-8")
        nl = "\r\n" if "\r\n" in raw else "\n"
        anchor, replacement = old.replace("\n", nl), new.replace("\n", nl)
        if raw.count(anchor) != 1:
            raise RuntimeError(f"{name}: anchor count {raw.count(anchor)}")
        path.write_bytes(raw.replace(anchor, replacement).encode("utf-8"))
        touch(path)
        with (core / f"batch53a-mutation-{name}.log").open("w") as log:
            summary = build_and_run(log)
            killed = summary is not None and summary[0] != 0
            log.write(f"\nMUTATION {name}: {'KILLED' if killed else 'SURVIVED'}\n")
        results.append((name, killed, summary))
        print(name, "KILLED" if killed else "SURVIVED", "build-failed" if summary is None else summary[1], flush=True)
        path.write_bytes(originals[rel])
        touch(path)
finally:
    for rel, data in originals.items():
        (root / rel).write_bytes(data)
        touch(root / rel)

with (core / "batch53a-mutation-summary.log").open("w") as f:
    for name, killed, summary in results:
        detail = "build-failed" if summary is None else f"exit={summary[0]} RED={len(summary[1])} [{' '.join(summary[1])}]"
        f.write(f"{name}: {'KILLED' if killed else 'SURVIVED'} -- {detail}\n")
    f.write(f"\n{sum(k for _, k, _ in results)}/{len(results)} killed\n")
with (core / "batch53a-post-mutation.log").open("w") as log:
    final = build_and_run(log)
    ok = final is not None and final[0] == 0
    log.write(f"\nPOST-MUTATION RESTORED BUILD: {'GREEN' if ok else 'NOT GREEN'}\n")
    print(f"post-mutation restored build: {'GREEN' if ok else 'NOT GREEN'}")
if not all(killed for _, killed, _ in results) or not ok:
    raise SystemExit(1)
