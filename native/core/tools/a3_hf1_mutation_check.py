"""Alpha 3 A3-HF1 mutation driver: each mutant must turn a3_hf1_arena_loot
(or combat_loot_open_regression) RED.

    python tools/a3_hf1_mutation_check.py <build-dir> [M1,M4,...]

Each mutation edits one production file, rebuilds the two targets, runs them,
restores the file byte for byte and touches it (a restored file is older than
the mutated object, so ninja would otherwise keep the mutant). A mutant that
does not build counts as INVALID, not killed.
"""
import os
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
TOOLS = "C:/Dev/TamaPoke/.build-tools/w64devkit/bin"
NINJA = TOOLS + "/ninja.exe"
PACK = str(ROOT.parent / "assets" / "openu5-alpha1-resources.bin")
TARGETS = ["a3_hf1_arena_loot", "combat_loot_open_regression"]

LOOT = "src/loot.cpp"
COMBAT = "src/combat.cpp"

MUTATIONS = [
    ("M1", "revert the fix: a full counter refuses the pickup again", LOOT,
     "auto add = [&](int32_t &n, int a) {if(a<=0)return false;if(n<99)n=std::min<int32_t>(99,n+a);return true;};",
     "auto add = [&](int32_t &n, int a) {if(a<=0||n>=99)return false;n=std::min<int32_t>(99,n+a);return true;};"),
    ("M2", "revert the fix for gold only", LOOT,
     "        if(q<=0)return false;\n        if(g.gold<9999)g.gold",
     "        if(q<=0||g.gold>=9999)return false;\n        if(g.gold<9999)g.gold"),
    ("M3", "skip reward generation (RNG still drawn)", COMBAT,
     "if (r30() <= d.treasure) {",
     "if (r30() <= d.treasure && false) {"),
    ("M4", "clear the chest's contents on Open", COMBAT,
     "chest ? std::max(0, int(s.chest_contents[key])) : s.piles[found].quantity & 127;",
     "chest ? 0 : s.piles[found].quantity & 127;"),
    ("M5", "shift the spilled reward one tile east", COMBAT,
     "gc.state.piles[gc.state.pile_count++] = {{int16_t(gc.x), int16_t(gc.y)},",
     "gc.state.piles[gc.state.pile_count++] = {{int16_t(gc.x + 1), int16_t(gc.y)},"),
    ("M6", "Get ignores the opened chest's loot on the ground", COMBAT,
     "            if (same(i) && (action == CombatAction::Get || s.piles[i].id == 1)) {",
     "            if (same(i) && action != CombatAction::Get && s.piles[i].id == 1) {"),
    ("M7", "drop the loot during Open (spilled, then lost)", COMBAT,
     "            if (gc.count) {\n                e.message(\"Found:\", a->id);",
     "            if (gc.count) {\n                s.pile_count -= gc.count;\n                e.message(\"Found:\", a->id);"),
    ("M8", "Get grants but never removes: duplicate loot", COMBAT,
     "                erase(found);\n                if (grant.id == 2)",
     "                if (grant.id == 2)"),
    ("M9", "Get takes the bottom of the stack (FIFO), not the top", COMBAT,
     "        for (int i = s.pile_count - 1; i >= 0; --i)\n            if (same(i) && (action == CombatAction::Get",
     "        for (int i = 0; i < s.pile_count; ++i)\n            if (found < 0 && same(i) && (action == CombatAction::Get"),
    ("M10", "a full counter overflows its cap", LOOT,
     "if(n<99)n=std::min<int32_t>(99,n+a);return true;",
     "n=n+a;return true;"),
    ("M11", "unknown records are silently consumed", LOOT,
     "    default:\n        return false;\n    }\n    return false;\n}",
     "    default:\n        return true;\n    }\n    return false;\n}"),
    ("M12", "duplicate the reward on teardown: promote the chest to the world", COMBAT,
     "    world.game.rng.seed(state.rng.get_seed());\n    world.combat = false;",
     "    for (int k = 0; k < kCombatCells; ++k) if (world.quest_world && world.quest_world->append && (state.loot[k] == 1 || state.loot[k] == 129 || state.chest_state[k] == CombatChestState::Consumed)) { QuestObject o{}; o.chest = true; o.x = state.loot_x; o.y = state.loot_y; world.quest_world->append(world.quest_world->context, o); }\n    world.game.rng.seed(state.rng.get_seed());\n    world.combat = false;"),
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
        path = ROOT / rel
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
                t = run([str(build / "a3_hf1_arena_loot.exe"), PACK], build, env, 600)
                red_lines = [l for l in t.stdout.splitlines() if l.startswith("RED ")]
                if t.returncode != 0:
                    reds.append("a3_hf1_arena_loot: " + "; ".join(l.split(" ")[1] for l in red_lines))
                c = run([str(build / "combat_loot_open_regression.exe")], build, env, 120)
                if c.returncode != 0:
                    reds.append("combat_loot_open_regression: " + (c.stderr.strip().splitlines() or ["exit"])[-1])
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
