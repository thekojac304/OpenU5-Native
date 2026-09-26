"""Batch 53B source mutations for the moongate return trip (Phase 7E-C).

Batch 53B changes no production code: the adjudication is Outcome A (the
device already does what the original does). The new guard,
batch53b_moongate_return, is therefore GREEN on first run, and a first-run
GREEN proves nothing. Each case below plants ONE of the defects the report
could have been -- a just-arrived flag, an anti-retrigger latch, a reverse
link, a drawn-but-inert gate, a wrong index, a double trigger, a clock jump --
rebuilds the guard, runs it on the shipped pack and requires at least one RED.

Production is restored byte for byte and touched after every case, so ninja
never reuses a mutated object (host-and-firmware-toolchain)."""
from pathlib import Path
import os
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[3]
core = root / "native/core"
build = core / (sys.argv[1] if len(sys.argv) > 1 else "build-batch53b")
bin_dir = Path("C:/Dev/TamaPoke/.build-tools/w64devkit/bin")
env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"])
pack = str(root / "native/assets/openu5-alpha1-resources.bin")
targets = ["batch53b_moongate_return"]

CMDS = "native/core/src/commands.cpp"
QW = "native/core/src/quest_world.cpp"
TR = "native/core/src/transitions.cpp"

GATE_TEST = "if(g.position.map.location || !q.moon_phases || !moongate_at(g,c.turn,q))return false;"
GATE_JUMP = "moonstone_teleport(g,c.turn,c.travel,q.moonstones,q.moonstone_count,phase,banner,transitions());return false;}"
GATE_LOC = "auto loc=q.moonstones[phase].location;auto banner=c.services.banner?c.services.banner(c.services.context,loc):nullptr;moonstone_teleport(g,"

# (name, file, [(anchor, replacement), ...]) -- every anchor must occur exactly once.
cases = [
    # A "just arrived" flag that is set by a transit and never cleared.
    ("just_arrived_never_clears", CMDS, [
        (GATE_TEST, "static bool b53b_arrived=false;if(b53b_arrived)return false;" + GATE_TEST),
        (GATE_JUMP, GATE_JUMP.replace("transitions());return false;}", "transitions());b53b_arrived=true;return false;}")),
    ]),
    # A same-tile anti-retrigger applied forever to the landing cell.
    ("same_tile_antiretrigger_forever", CMDS, [
        (GATE_TEST, "static int b53b_lx=-1,b53b_ly=-1;if(g.position.xy.x==b53b_lx&&g.position.xy.y==b53b_ly)return false;" + GATE_TEST),
        (GATE_JUMP, GATE_JUMP.replace("transitions());return false;}",
                                      "transitions());b53b_lx=g.position.xy.x;b53b_ly=g.position.xy.y;return false;}")),
    ]),
    # The tester's mental model: the arrival gate leads back to the origin gate.
    ("reverse_link_to_origin", CMDS, [
        (GATE_LOC, "static int b53b_from=-1,b53b_to=-1;int b53b_here=-1;"
                   "for(size_t i=0;i<q.moonstone_count;++i)if(q.moonstones[i].x==g.position.xy.x&&q.moonstones[i].y==g.position.xy.y)b53b_here=int(i);"
                   "if(b53b_here==b53b_to&&b53b_from>=0)phase=b53b_from;b53b_from=b53b_here;b53b_to=phase;" + GATE_LOC),
    ]),
    # Drawn but inert: the active phase's own gate is visible and never fires.
    ("visible_but_inactive", QW, [
        ("    return buried_stone_at(g,s,g.position.xy.x,g.position.xy.y);\n}",
         "    {int p=active_gate_phase(g,t,s);if(p>=0&&size_t(p)<s.moonstone_count&&s.moonstones[p].x==g.position.xy.x&&s.moonstones[p].y==g.position.xy.y)return false;}\n"
         "    return buried_stone_at(g,s,g.position.xy.x,g.position.xy.y);\n}"),
    ]),
    # Active but not drawn: the arrival gate disappears from the view.
    ("active_but_invisible", QW, [
        ("    return !g.position.map.location && s.moon_phases && active_gate_phase(g,t,s)>=0 && buried_stone_at(g,s,x,y);",
         "    {int p=active_gate_phase(g,t,s);if(p>=0&&size_t(p)<s.moonstone_count&&s.moonstones[p].x==x&&s.moonstones[p].y==y)return false;}\n"
         "    return !g.position.map.location && s.moon_phases && active_gate_phase(g,t,s)>=0 && buried_stone_at(g,s,x,y);"),
    ]),
    # Wrong destination index.
    ("wrong_destination_index", CMDS, [
        (GATE_JUMP, GATE_JUMP.replace("q.moonstone_count,phase,banner", "q.moonstone_count,(phase+1)%8,banner")),
    ]),
    # Two activations on a single step.
    ("double_trigger_per_step", CMDS, [
        ("        effect(CommandEffect::Moongate);\n        effect(CommandEffect::ShrineEntry);",
         "        effect(CommandEffect::Moongate);\n        effect(CommandEffect::Moongate);\n        effect(CommandEffect::ShrineEntry);"),
    ]),
    # The transit moves the clock (0x47f4 / 0x48a8 never do).
    ("transit_advances_clock", TR, [
        ("    const auto dest = stones[phase];\n",
         "    const auto dest = stones[phase];\n    g.time.hour = (g.time.hour + 3) % 24;\n"),
    ]),
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
        red += [l.split()[1] for l in lines if l.strip().startswith("RED ")]
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
    for name, rel, edits in cases:
        path = root / rel
        if rel not in originals:
            originals[rel] = path.read_bytes()
        raw = originals[rel].decode("utf-8")
        nl = "\r\n" if "\r\n" in raw else "\n"
        for old, new in edits:
            anchor, replacement = old.replace("\n", nl), new.replace("\n", nl)
            if raw.count(anchor) != 1:
                raise RuntimeError(f"{name}: anchor count {raw.count(anchor)}")
            raw = raw.replace(anchor, replacement)
        path.write_bytes(raw.encode("utf-8"))
        touch(path)
        with (core / f"batch53b-mutation-{name}.log").open("w") as log:
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

with (core / "batch53b-mutation-summary.log").open("w") as f:
    for name, killed, summary in results:
        detail = "build-failed" if summary is None else f"exit={summary[0]} RED={len(summary[1])} [{' '.join(summary[1])}]"
        f.write(f"{name}: {'KILLED' if killed else 'SURVIVED'} -- {detail}\n")
    f.write(f"\n{sum(k for _, k, _ in results)}/{len(results)} killed\n")
with (core / "batch53b-post-mutation.log").open("w") as log:
    final = build_and_run(log)
    ok = final is not None and final[0] == 0
    log.write(f"\nPOST-MUTATION RESTORED BUILD: {'GREEN' if ok else 'NOT GREEN'}\n")
    print(f"post-mutation restored build: {'GREEN' if ok else 'NOT GREEN'}")
if not all(killed for _, killed, _ in results) or not ok:
    raise SystemExit(1)
