from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[3]
source = root / "native/core/src/rest.cpp"
build = root / "native/core/build-batch40"
out = root / "native/core"
cmake = Path("C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe")
exe = build / "batch40_advancement.exe"
os.environ["PATH"] = "C:/Dev/TamaPoke/.build-tools/w64devkit/bin;" + os.environ["PATH"]
original = source.read_bytes()
text = source.read_text(encoding="utf-8")
cases = [
    ("wrong_xp_divisor", "m.exp / 100", "m.exp / 101"),
    ("always_apparition", "if (camp_hole_up(c.game, c.rand, guard)) {", "if (camp_hole_up(c.game, c.rand, guard) || true) {"),
    ("first_member_only", "for (int32_t i = 0; i < count(c.game); ++i) {\n            auto &m = c.game.party.characters[i];\n            if (m.status == 'D')", "for (int32_t i = 0; i < std::min<int32_t>(1, count(c.game)); ++i) {\n            auto &m = c.game.party.characters[i];\n            if (m.status == 'D')"),
    ("reverse_roster", "for (int32_t i = 0; i < count(c.game); ++i) {\n            auto &m = c.game.party.characters[i];\n            if (m.status == 'D')", "for (int32_t i = count(c.game) - 1; i >= 0; --i) {\n            auto &m = c.game.party.characters[i];\n            if (m.status == 'D')"),
    ("wrong_hp", "m.max_hp = uint16_t(30 * level);", "m.max_hp = uint16_t(30 * (level - 1));"),
    ("extra_stat_draw", "if (level != m.level) {", "if ((void)c.rand(1, 3), level != m.level) {"),
    ("erase_xp", "m.level = uint8_t(level);", "m.level = uint8_t(level); m.exp = 0;"),
    ("bed_levels", "bed_sleep_end(c);\n    return r;", "bed_sleep_end(c);\n    camp_wake(c);\n    return r;"),
]
summary = []
try:
    for name, old, new in cases:
        if text.count(old) != 1:
            raise RuntimeError(f"{name}: anchor count {text.count(old)}")
        source.write_text(text.replace(old, new), encoding="utf-8")
        with (out / f"batch40-mutation-{name}-build.log").open("w", encoding="utf-8") as log:
            b = subprocess.run([str(cmake), "--build", str(build), "--target", "batch40_advancement", "-j", "6"], stdout=log, stderr=subprocess.STDOUT)
        if b.returncode == 0:
            with (out / f"batch40-mutation-{name}.log").open("w", encoding="utf-8") as log:
                t = subprocess.run([str(exe)], stdout=log, stderr=subprocess.STDOUT)
            verdict = "KILLED" if t.returncode else "SURVIVED"
            summary.append(f"{name}: {verdict}, test exit {t.returncode}")
        else:
            summary.append(f"{name}: BUILD FAILED {b.returncode}")
        source.write_bytes(original)
finally:
    source.write_bytes(original)
    with (out / "batch40-mutations.log").open("w", encoding="utf-8") as log:
        log.write("\n".join(summary) + "\n")
    for line in summary:
        print(line)
