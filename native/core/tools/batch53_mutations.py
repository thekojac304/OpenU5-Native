"""Batch 53 source mutations for the four Alpha 2 release blockers (RB-1 .. RB-4)
and the shared production binders. Each case breaks ONE thing the blocker fix
depends on, rebuilds batch53_release_blockers, runs it against the shipped pack
and counts its RED checks. Production is always restored byte-for-byte and
touched afterwards so ninja never reuses a mutated object (a restored file
older than its object is otherwise skipped -- host-and-firmware-toolchain)."""
from pathlib import Path
import os
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[3]
core = root / "native/core"
build = core / (sys.argv[1] if len(sys.argv) > 1 else "build-batch53")
bin_dir = Path("C:/Dev/TamaPoke/.build-tools/w64devkit/bin")
env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"])
pack = str(root / "native/assets/openu5-alpha1-resources.bin")

RT = "native/targets/tdeck/main/alpha_runtime.cpp"
RES = "native/targets/tdeck/main/alpha_resources.cpp"
GEN = "native/targets/tdeck/main/alpha_save_generation.cpp"
FIX = "native/targets/tdeck/host_tests/alpha_runtime_host_fixture.cpp"
QUEST = "native/core/src/quest.cpp"
QW = "native/core/src/quest_world.cpp"
COMBAT = "native/core/src/combat.cpp"
PERSIST = "native/core/src/persistence.cpp"
UI = "native/core/src/ui_session.cpp"
CMDS = "native/core/src/commands.cpp"
PRES = "native/core/src/presentation.cpp"
TERRAIN = "native/core/src/world_terrain.cpp"
SHOPS = "native/core/src/shops.cpp"

cases = [
    # ---- RB-1 endgame -------------------------------------------------
    ("endgame_unbind_end_record", RT,
     "    // H-190: the six KARMA.DAT records, quoted at use",
     "    quest_.end_record=nullptr;\n    // H-190: the six KARMA.DAT records, quoted at use"),
    ("endgame_wrong_endmsg_record", RT,
     "r.resources_.end_text_record_count},index);};",
     "r.resources_.end_text_record_count},index+1);};"),
    ("endgame_suppress_game_won", QUEST,
     "    set_quest_flag(g.quest,QuestFlag::GameWon);\n    return g.wooden_box",
     "    return g.wooden_box"),
    ("endgame_combat_active_after_success", COMBAT,
     "world.game.rng.seed(state.rng.get_seed());world.combat=false;world.combat_context=nullptr;state.initialized=false;\n"
     "        emit(GameEventKind::CombatEnded);absorption_endgame",
     "world.game.rng.seed(state.rng.get_seed());\n        emit(GameEventKind::CombatEnded);absorption_endgame"),
    ("karma_invented_text", RT,
     "    rest_owner.karma_record=karma_speech;context_.rest_services=&rest_owner;",
     "    rest_owner.karma_record=[](void*,int32_t){return \"\\\"Rest well, Avatar. Continue upon the path of virtue.\\\"\";};"
     "context_.rest_services=&rest_owner;quest_.karma_record=rest_owner.karma_record;"),
    # ---- RB-2 Words of Power ------------------------------------------
    ("words_empty_table", RT,
     "    quest_.words={resources_.words,resources_.word_count};",
     "    quest_.words={resources_.words,0};"),
    ("words_wrong_dungeon_mapping", RES,
     "            o.words[i]={o.word_text+at,length};",
     "            o.words[(i+1)%8]={o.word_text+at,length};"),
    ("words_case_sensitive_match", QUEST,
     "if (contains(said,words[i])) {",
     "if (said.find(words[i])!=TalkText::npos) {"),
    ("words_wrong_seal_bit", QW,
     "if(r.opened){auto f=QuestFlag(int(QuestFlag::Word33)+r.location-33);",
     "if(r.opened){auto f=QuestFlag(int(QuestFlag::Word33)+(r.location-32)%8);"),
    ("words_seal_not_persisted", PERSIST,
     "(v.kind == J::Bool && v.truth() ? 128 : 0)",
     "(v.kind == J::Bool && v.truth() ? 0 : 0)"),
    # ---- RB-3 moonstones ----------------------------------------------
    ("moon_zero_count", RT,
     "    quest_.moonstones=moonstones_;quest_.moonstone_count=8;",
     "    quest_.moonstones=moonstones_;quest_.moonstone_count=0;"),
    ("moon_wrong_phase_digit", UI,
     "cmd.hours = int16_t(a.character - u'1');",
     "cmd.hours = int16_t(a.character - u'0');"),
    ("moon_wrong_transit_destination", CMDS,
     "moonstone_teleport(g,c.turn,c.travel,q.moonstones,q.moonstone_count,phase,banner,transitions());return false;}",
     "moonstone_teleport(g,c.turn,c.travel,q.moonstones,q.moonstone_count,(phase+1)&7,banner,transitions());return false;}"),
    ("moon_not_persisted", GEN,
     "openu5::save::capture_world_objects(*c.quest_world,retained);openu5::save::capture_moonstones(*c.quest_world,retained);",
     "openu5::save::capture_world_objects(*c.quest_world,retained);"),
    ("moon_gate_invisible", PRES,
     "if(moongate_visible_at(c.game,c.turn,*c.quest_world,m.x,m.y))place(m.x,m.y,kMoongateTile);",
     "if(moongate_visible_at(c.game,c.turn,*c.quest_world,m.x,m.y))(void)m;"),
    # ---- RB-4 shops ---------------------------------------------------
    ("shop_null_ship", RT,
     "    shop_services_.transactional_services=true;\n}",
     "    shop_services_.transactional_services=true;\n    shop_services_.ship=nullptr;\n}"),
    ("shop_null_horse", RT,
     "    shop_services_.transactional_services=true;\n}",
     "    shop_services_.transactional_services=true;\n    shop_services_.horse=nullptr;\n}"),
    ("shop_null_reserve", RT,
     "    shop_services_.transactional_services=true;\n}",
     "    shop_services_.transactional_services=true;\n    shop_services_.reserve=nullptr;\n}"),
    ("shop_charge_without_transport", TERRAIN,
     "    auto *q=c.quest_world;if(!q||!q->append)return;",
     "    auto *q=c.quest_world;if(q||!q)return;"),
    ("shop_transport_without_charge", SHOPS,
     "        return range();\n    pay(g, price);\n    ShopResult r{true, \"She awaits thee at the dock!\"};",
     "        return range();\n    ShopResult r{true, \"She awaits thee at the dock!\"};"),
    # ---- the binders --------------------------------------------------
    ("binder_fixture_bypasses_production", FIX,
     "    bind_shop_services();\n",
     "    shop_services_.context = this;\n"
     "    shop_services_.tile = [](void *p, int32_t x, int32_t y) { return tile_at(p, x, y); };\n"
     "    shop_services_.occupied = [](void *, int32_t, int32_t) { return false; };\n"
     "    shop_services_.transactional_services = true;\n"),
    ("binder_device_only_hook_missing", RT,
     "    bind_quest_services();\n    dialogue_assets_.bind(",
     "    dialogue_assets_.bind("),
]


def build_and_run(log):
    result = subprocess.run([str(bin_dir / "cmake.exe"), "--build", str(build), "--target", "batch53_release_blockers"],
                            cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        log.write("\nBUILD FAILED\n")
        return None
    out = subprocess.run([str(build / "batch53_release_blockers.exe"), pack], cwd=build, env=env,
                         capture_output=True, text=True, errors="replace", timeout=600)
    text = out.stdout + out.stderr
    lines = [l for l in text.splitlines() if not l.startswith("[")]
    red = [l.split()[1] for l in lines if l.strip().startswith("RED ")]
    log.write("\n".join(l for l in lines if l.strip().startswith(("RED ", "GREEN ")) or "checks" in l))
    log.write(f"\n==== exit={out.returncode} red={len(red)}\n")
    return out.returncode, red


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
        with (core / f"batch53-mutation-{name}.log").open("w") as log:
            summary = build_and_run(log)
            killed = summary is not None and summary[0] != 0
            log.write(f"\nMUTATION {name}: {'KILLED' if killed else 'SURVIVED'}\n")
        results.append((name, killed, summary))
        print(name, "KILLED" if killed else "SURVIVED", "" if summary is None else summary[1], flush=True)
        path.write_bytes(originals[rel])
        touch(path)
finally:
    for rel, data in originals.items():
        (root / rel).write_bytes(data)
        touch(root / rel)

with (core / "batch53-mutation-summary.log").open("w") as f:
    for name, killed, summary in results:
        detail = "build-failed" if summary is None else f"exit={summary[0]} RED={len(summary[1])} [{' '.join(summary[1])}]"
        line = f"{name}: {'KILLED' if killed else 'SURVIVED'} -- {detail}"
        f.write(line + "\n")
    f.write(f"\n{sum(k for _, k, _ in results)}/{len(results)} killed\n")
with (core / "batch53-post-mutation.log").open("w") as log:
    final = build_and_run(log)
    ok = final is not None and final[0] == 0
    log.write(f"\nPOST-MUTATION RESTORED BUILD: {'GREEN' if ok else 'NOT GREEN'}\n")
    print(f"post-mutation restored build: {'GREEN' if ok else 'NOT GREEN'}")
if not all(killed for _, killed, _ in results):
    raise SystemExit(1)
