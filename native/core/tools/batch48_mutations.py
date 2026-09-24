"""Temporary source mutations for Batch 48; always restores production."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[3]
source = root / "native/core/src/presentation.cpp"
out = root / "native/core"
build = root / "native/core/build-batch48"
bin_dir = Path("C:/Dev/TamaPoke/.build-tools/w64devkit/bin")
env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"])
original = source.read_bytes()
text = original.decode()
cases = {
    "omit": ("const bool bed_actor = avatar_tile >= 0x100", "const bool bed_actor = false && avatar_tile >= 0x100"),
    "right_half": ("== 0xab ? 0x11a", "== 0xac ? 0x11a"),
    "camp_tile": ("== 0xab ? 0x11a", "== 0xab ? 0x11e"),
    "all_actors": ("actor_byte == 0x1c ||", "actor_byte >= 0x1c ||"),
    "drop_foot": ("actor_byte == 0x1c ||", "actor_byte == 0x11 ||"),
    "drop_class": ("(actor_byte >= 0x40 && actor_byte < 0x80)", "false"),
    "npc_pose": ("place(a.x,a.y,a.schedule.type+256", "place(a.x,a.y,0x11a"),
    "object_layer": ("c.terrain->effective(c.world,map.id,center.x,center.y)", "effective_terrain(c,map,center.x,center.y)"),
}
results = []
try:
    for name, (old, new) in cases.items():
        if text.count(old) != 1:
            raise RuntimeError(f"{name}: anchor count {text.count(old)}")
        source.write_text(text.replace(old, new))
        log = out / f"batch48-mutation-{name}.log"
        with log.open("w") as f:
            build_result = subprocess.run([str(bin_dir / "cmake.exe"), "--build", str(build),
                                           "--target", "batch29_rest_wiring", "-j", "4"],
                                          cwd=root, env=env, stdout=f, stderr=subprocess.STDOUT)
            if build_result.returncode == 0:
                test_result = subprocess.run([str(bin_dir / "ctest.exe"), "--test-dir", str(build),
                                              "-R", "^batch48_bed_pose$", "--output-on-failure"],
                                             cwd=root, env=env, stdout=f, stderr=subprocess.STDOUT)
                killed = test_result.returncode != 0
            else:
                killed = False
            f.write(f"\nMUTATION {name}: {'KILLED' if killed else 'SURVIVED'}\n")
        results.append((name, killed))
        source.write_bytes(original)
finally:
    source.write_bytes(original)
for name, killed in results:
    print(f"{name}: {'KILLED' if killed else 'SURVIVED'}")
if not all(killed for _, killed in results):
    raise SystemExit(1)