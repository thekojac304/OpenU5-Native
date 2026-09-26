"""Batch 51 source mutations: each one removes or moves ONE original wait (or
the input rule that protects it), rebuilds the affected targets, runs them and
counts the RED checks. Production is always restored byte-for-byte and touched
afterwards so ninja never reuses a mutated object (host-and-firmware-toolchain
memory: a restored file older than its object is otherwise skipped)."""
from pathlib import Path
import os
import subprocess
import time

root = Path(__file__).resolve().parents[3]
core = root / "native/core"
build = core / "build-batch51"
bin_dir = Path("C:/Dev/TamaPoke/.build-tools/w64devkit/bin")
env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"])
pack = str(root / "native/assets/openu5-alpha1-resources.bin")
strings = str(root / "game/assets/ds-strings.json")

TIMING = "native/core/src/scene_timing.cpp"
TIMING_H = "native/core/include/openu5/scene_timing.h"
PACER = "native/core/src/narrative_scene.cpp"
BT = "native/core/src/blackthorn_scene.cpp"
RT = "native/targets/tdeck/main/alpha_runtime.cpp"

cases = [
    ("chord_hold_zero", TIMING,
     "if (cue(e, kApparitionChordCue)) return tone_sweep_ms(kApparitionChordSamples);",
     "if (cue(e, kApparitionChordCue)) return 0;"),
    ("restore_frames_zero", TIMING,
     "return run_n_frames_ms(kApparitionRestoreFrames);",
     "return 0;"),
    ("figure_fizzle_zero", TIMING,
     "        return kFizzleFloorMs;\n    case GameEventKind::CampActorWake:",
     "        return 0;\n    case GameEventKind::CampActorWake:"),
    ("pacer_ignores_forward_dwell", PACER,
     "        if (step.dwell_ms) {\n            // Batch 51",
     "        if (false && step.dwell_ms) {\n            // Batch 51"),
    ("dwell_before_publication", PACER,
     "        if (forward.emit) forward.emit(forward.context, released);\n        if (step.dwell_ms) {",
     "        if (step.dwell_ms && !step.has_sfx) { step.has_sfx = true; resume_at_ms_ = now_ms + step.dwell_ms;"
     " waiting_ = true; ++count_; --released_; head_ = (head_ + storage_.step_capacity - 1) %"
     " storage_.step_capacity; return; }\n        if (forward.emit) forward.emit(forward.context, released);\n"
     "        if (false) {"),
    ("sweep_primitive_zero", TIMING_H,
     "    return uint32_t((uint64_t(samples) * 1000u) / kToneSweepSamplesPerSecond);",
     "    return uint32_t(samples * 0u);"),
    ("bt_materialize_sweep_zero", BT,
     "    sweep.sweep_samples = kBlackthornMaterializeSamples;",
     "    sweep.sweep_samples = 0;"),
    ("bt_circle_fizzle_zero", BT,
     "    circle.fizzle = true;",
     "    circle.fizzle = false;"),
    ("bt_siren_zero", BT,
     "    siren.sweep_samples = kBlackthornSirenSamples;",
     "    siren.sweep_samples = 0;"),
    ("bt_circle_during_sweep", BT,
     "    auto &sweep = b.emit();\n    sweep.sfx = BlackthornSfx::Materialize;",
     "    state.objects[8] = {kBlackthornCellX, kBlackthornCellY, kBlackthornHolySymbolTile, true, true};\n"
     "    auto &sweep = b.emit();\n    b.stamp(sweep);\n    sweep.sfx = BlackthornSfx::Materialize;"),
    ("camp_dwell_input_open", RT,
     "    if(narrative_pacer_.modal()&&narrative_pacer_.scene()==openu5::NarrativeScene::Camp){",
     "    if(false&&narrative_pacer_.modal()&&narrative_pacer_.scene()==openu5::NarrativeScene::Camp){"),
    ("camp_paced_in_harness", PACER,
     "        if (!paced_ || !camp_apparition_event(e) || !camp_apparition_wait_ms(e)) return false;",
     "        if (!camp_apparition_event(e) || !camp_apparition_wait_ms(e)) return false;"),
]

targets = ["batch51_scene_pacing", "batch51_camp_pacing", "batch42_advancement_flash",
           "blackthorn_scene_tests", "batch7b_tests"]
runs = [
    ("batch51_scene_pacing", ["batch51_scene_pacing.exe"]),
    ("batch51_camp_pacing", ["batch51_camp_pacing.exe", pack]),
    ("batch42_advancement_flash", ["batch42_advancement_flash.exe", pack]),
    ("blackthorn_scene", ["blackthorn_scene_tests.exe", strings, pack]),
    ("batch7b", ["batch7b_tests.exe"]),
]


def build_and_run(log):
    result = subprocess.run([str(bin_dir / "cmake.exe"), "--build", str(build), "--target", *targets],
                            cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        log.write("\nBUILD FAILED\n")
        return None
    summary = {}
    for name, cmd in runs:
        out = subprocess.run([str(build / cmd[0]), *cmd[1:]], cwd=build, env=env,
                             capture_output=True, text=True, errors="replace")
        text = out.stdout + out.stderr
        red = sum(1 for line in text.splitlines() if line.startswith("RED "))
        log.write(f"\n==== {name} exit={out.returncode} red={red}\n")
        log.write("\n".join(l for l in text.splitlines() if l.startswith(("RED ", "GREEN ")) or "checks" in l
                            or "failed" in l))
        summary[name] = (out.returncode, red)
    return summary


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
        with (core / f"batch51-mutation-{name}.log").open("w") as log:
            summary = build_and_run(log)
            killed = summary is not None and any(code != 0 for code, _ in summary.values())
            log.write(f"\nMUTATION {name}: {'KILLED' if killed else 'SURVIVED'}\n")
        results.append((name, killed, summary))
        path.write_bytes(originals[rel])
        touch(path)
finally:
    for rel, data in originals.items():
        (root / rel).write_bytes(data)
        touch(root / rel)

with (core / "batch51-mutation-summary.log").open("w") as f:
    for name, killed, summary in results:
        detail = "build-failed" if summary is None else ", ".join(
            f"{t}:{'RED' if c else 'green'}({r})" for t, (c, r) in summary.items())
        line = f"{name}: {'KILLED' if killed else 'SURVIVED'} -- {detail}"
        print(line)
        f.write(line + "\n")
# Post-mutation rebuild so the tree is left on restored production objects.
with (core / "batch51-post-mutation.log").open("w") as log:
    final = build_and_run(log)
    ok = final is not None and all(code == 0 for code, _ in final.values())
    log.write(f"\nPOST-MUTATION RESTORED BUILD: {'GREEN' if ok else 'NOT GREEN'}\n")
    print(f"post-mutation restored build: {'GREEN' if ok else 'NOT GREEN'}")
if not all(killed for _, killed, _ in results):
    raise SystemExit(1)
