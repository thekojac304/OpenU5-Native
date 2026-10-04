r"""A4-PARITY2 (ALPHA4_UI.md section 16) -- mutation check of every fix: each mutant edits one (or a few)
production anchors, rebuilds, runs the checks that pin the item and must turn at least one RED.
A native mutant that does not build is INVALID, not killed.

    python tools/a4_parity2_mutation_check.py <build-dir> [ITEM|ID[,ID...]]

ITEM is a group name (D89, D83, D85, ...); with no second argument every mutant of every group runs.
N* mutate native production (rebuilt; ctest on the group's tests), T* mutate the TypeScript reference
(vitest on the group's suites, the group's corpus generators with --check, and the live parity tests
against the unchanged native).

Anchors are written with \n and matched in each file's own line endings
(checkout-line-endings-are-mixed); every file is restored and touched after each mutant, and the restored
tree is re-run at the end (it must be GREEN, or the whole run fails).
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
BIN = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin'
NODE = 'C:/Program Files/nodejs/node.exe'
SRC = 'native/core/src/'
TS = 'game/src/core/'

# group -> what to run
GROUPS = {
    'D89': dict(
        native_targets=['a4_parity2_d89_naval_ouch', 'a4_enh2_rules', 'a4_enh1_rules', 'movement_flow_parity_tests',
                        'gameplay_driver', 'quest_driver', 'turn_parity_tests'],
        native_tests=['a4_parity2_d89_naval_ouch', 'a4_enh2_rules', 'a4_enh1_rules', 'movement_flow_parity', 'gameplay_parity',
                      'quest_parity', 'turn_parity'],
        vitest=['tests/a4-parity2-d89-naval-ouch.test.ts', 'tests/naval-live.test.ts', 'tests/cactus-ouch.test.ts',
                'tests/transport-exact.test.ts'],
        generators=[('generate-movement-flow-fixtures.ts', ['--check']), ('generate-turn-fixtures.ts', ['--check'])],
    ),
    'D83': dict(
        native_targets=['a4_parity2_d83_d84_resurrect', 'shop_parity_tests', 'shop_flow_tests', 'quest_driver', 'magic_parity_tests',
                        'batch7b_tests', 'a4_enh2_preservation', 'gameplay_driver'],
        native_tests=['a4_parity2_d83_d84_resurrect', 'shop_parity', 'shop_flow', 'quest_parity', 'magic_parity', 'batch7b',
                      'a4_enh2_preservation', 'gameplay_parity'],
        vitest=['tests/a4-parity2-d83-d84-resurrect.test.ts', 'tests/shops.test.ts', 'tests/blackthorn.test.ts', 'tests/magic.test.ts',
                'tests/cast-onwho-consumidores.test.ts'],
        generators=[('generate-shop-fixtures.ts', ['--check']), ('generate-shop-flow-fixtures.ts', ['--check']),
                    ('generate-magic-fixtures.ts', ['--check'])],
    ),
    'D85': dict(
        native_targets=['a4_parity2_d85_trapdoor_party', 'a3_03_sfx_runtime', 'turn_parity_tests', 'command_parity_tests'],
        native_tests=['a4_parity2_d85_trapdoor_party', 'a3_03_sfx_runtime', 'turn_parity', 'command_parity'],
        vitest=['tests/a4-parity2-d85-trapdoor-party.test.ts', 'tests/trapdoor-fall.test.ts'],
        generators=[('generate-turn-fixtures.ts', ['--check'])],
    ),
    'D86': dict(
        native_targets=['a4_parity2_d86_dungeon_digit_runtime', 'gameplay_driver', 'batch15_sacrifice_tests'],
        native_tests=['a4_parity2_d86_dungeon_digit_runtime', 'gameplay_parity', 'batch15_sacrifice'],
        vitest=['tests/a4-parity2-d86-dungeon-digit.test.ts', 'tests/game.test.ts'],
        generators=[],
    ),
    'D87': dict(
        native_targets=['a4_parity2_d87_crop_food_cap', 'gameplay_driver', 'quest_driver', 'a4_enh2_rules'],
        native_tests=['a4_parity2_d87_crop_food_cap', 'gameplay_parity', 'quest_parity', 'a4_enh2_rules'],
        vitest=['tests/a4-parity2-d87-crop-cap.test.ts', 'tests/get-plates-direction-gate.test.ts', 'tests/get-torch-arena-375.test.ts'],
        generators=[],
    ),
    'D88': dict(
        native_targets=['a4_parity2_d88_combat_clock', 'gameplay_driver', 'quest_driver', 'persistence_driver', 'a4_enh1_preservation',
                        'a4_enh2_preservation', 'a4_enh1_preservation_runtime', 'a4_enh2_preservation_runtime', 'a4_save3_pc_bridge_runtime'],
        native_tests=['a4_parity2_d88_combat_clock', 'gameplay_parity', 'quest_parity', 'persistence_parity', 'a4_enh1_preservation',
                      'a4_enh2_preservation', 'a4_enh1_preservation_runtime', 'a4_enh2_preservation_runtime', 'a4_save3_pc_bridge_runtime'],
        vitest=['tests/a4-parity2-d88-combat-clock.test.ts'],
        generators=[('generate-combat-fixtures.ts', ['--check']), ('generate-advanced-combat-fixtures.ts', ['--check'])],
    ),
    'D82': dict(
        native_targets=['a4_parity2_d82_runtime', 'a4_save3_pc_bridge_runtime', 'a4_save2_slots_runtime', 'gameplay_driver', 'quest_driver'],
        native_tests=['a4_parity2_d82_runtime', 'a4_save3_pc_bridge_runtime', 'a4_save2_slots_runtime', 'gameplay_parity', 'quest_parity'],
        vitest=['tests/a4-parity2-d82-underworld-seed.test.ts'],
        generators=[],
    ),
}

MUTANTS = {
    # ---------------------------------------------------------------- D-82 native
    'N80': ('D82', 'the New Journey never calls the seed', [('native/targets/tdeck/main/alpha_runtime.cpp',
            'ok=openu5::seed_new_journey_underworld(resources_.initial_ool,resources_.initial_ool_size,quest_,terrain_);', 'ok=true;')]),
    'N81': ('D82', 'a load seeds too (not only the New Journey seam)', [('native/targets/tdeck/main/alpha_runtime.cpp',
            'if(openu5::save::restore_world_objects(retained_,quest_)!=openu5::save::Error::None)ESP_LOGW(kTag,"WORLD_OBJECTS_RESTORE_FAILED',
            'openu5::seed_new_journey_underworld(resources_.initial_ool,resources_.initial_ool_size,quest_,terrain_);if(openu5::save::restore_world_objects(retained_,quest_)!=openu5::save::Error::None)ESP_LOGW(kTag,"WORLD_OBJECTS_RESTORE_FAILED')]),
    'N82': ('D82', 'a body lands on floor 0, not the underworld', [(SRC + 'quest_world.cpp', 'o.location=0;o.floor=r.floor;', 'o.location=0;o.floor=0;')]),
    'N83': ('D82', 'a body is a plain object, not a prop', [(SRC + 'quest_world.cpp', 'else o.prop=true;', 'else o.prop=false;')]),
    'N84': ('D82', 'the skiff override is transient (a save would drop it)', [(SRC + 'quest_world.cpp', 'tile,true,"seed.new-journey"', 'tile,false,"seed.new-journey"')]),
    'N85': ('D82', 'the floor-vs-block check is gone', [(SRC + 'quest_world.cpp', 'if(!r[0]||r[4]!=floor)continue;', 'if(!r[0])continue;')]),
    'N86': ('D82', 'a refused reservation is ignored', [(SRC + 'quest_world.cpp',
            'if(objects&&(!pool(&s)||!s.reserve(s.context,objects)))return false;', 'if(objects&&!pool(&s))return false;\n    if(objects){s.reserve(s.context,objects);}')]),
    'N87': ('D82', 'slot 0 (the party\'s own vehicle) is seeded', [(SRC + 'quest_world.cpp', 'for(int slot=1;slot<32;++slot){', 'for(int slot=0;slot<32;++slot){')]),
    'N88': ('D82', 'the skiff tile is off by one', [(SRC + 'quest_world.cpp', 'const int32_t tile=0x100+r.b;', 'const int32_t tile=0x101+r.b;')]),
    'N89': ('D82', 'a frigate loses its hull', [(SRC + 'quest_world.cpp', 'o.ship=true;o.hull=r.hull;', 'o.ship=true;o.hull=0;')]),
    'N90': ('D82', 'an unknown class byte is placed as a prop', [(SRC + 'quest_world.cpp',
            'if(!terrain_class&&!ship&&!body)continue;', 'if(!terrain_class&&!ship&&!body&&!b)continue;')]),
    # ---------------------------------------------------------------- D-82 reference
    'T80': ('D82', 'reference: the floor-vs-block check is gone', [(TS + 'state.ts', 'if ((ool[o + 4] ?? 0) !== floor) {', 'if (false) {')]),
    'T81': ('D82', 'reference: the skiff goes on floor 0', [(TS + 'state.ts', '[`0:${floor}:${x}:${y}`] = tile;', '[`0:0:${x}:${y}`] = tile;')]),
    'T82': ('D82', 'reference: slot 0 is seeded', [(TS + 'state.ts', 'for (let slot = 1; slot < 32; slot++) {', 'for (let slot = 0; slot < 32; slot++) {')]),
    'T83': ('D82', 'reference: an unknown class byte is placed as a prop', [(TS + 'state.ts', 'report.unknown.push(b);',
            '(state.worldObjects ??= []).push({ location: 0, floor, x, y, tile, kind: "prop", slot }); report.unknown.push(b);')]),
    'T84': ('D82', 'reference: a frigate loses its hull', [(TS + 'state.ts', 'hull: ool[o + 5]!, ', 'hull: 0, ')]),
    # ---------------------------------------------------------------- D-89 native
    'N1': ('D89', 'naval OUCH rolls once on the active member again', [(SRC + 'commands.cpp',
           '            party_random_damage(c.game, rand);\n            event(GameEventKind::PartyChanged); // K:2A52',
           '            { const auto d = rand(1, 8); auto i = c.game.party.active_character; if (i >= c.game.party.character_count) i = 0;\n'
           '              auto &hp = c.game.party.characters[i].current_hp; hp = uint16_t(hp > d ? hp - d : 0); }\n'
           '            event(GameEventKind::PartyChanged); // K:2A52')]),
    'N2': ('D89', 'naval OUCH does not tell the party panel', [(SRC + 'commands.cpp',
           '            event(GameEventKind::PartyChanged); // K:2A52 ends in the K:2900 party-panel redraw\n', '')]),
    'N3': ('D89', 'the helper draws BEFORE it skips a dead member', [(SRC + 'turn.cpp',
           "        if (i < g.party.character_count && g.party.characters[i].status == 'D') continue;\n"
           "        const int32_t d = rand(1,8);",
           "        const int32_t d = rand(1,8);\n"
           "        if (i < g.party.character_count && g.party.characters[i].status == 'D') continue;")]),
    'N4': ('D89', 'the helper stops at five members', [(SRC + 'turn.cpp',
           'for (int32_t i = 0; i < g.party.party_size && i < 6; ++i) {\n        if (i < g.party.character_count',
           'for (int32_t i = 0; i < g.party.party_size && i < 5; ++i) {\n        if (i < g.party.character_count')]),
    'N5': ('D89', 'God Mode gates the naval OUCH before the draws', [(SRC + 'commands.cpp',
           '            party_random_damage(c.game, rand);\n            event(GameEventKind::PartyChanged); // K:2A52',
           '            if (!party_damage_blocked(c.game)) party_random_damage(c.game, rand);\n            event(GameEventKind::PartyChanged); // K:2A52')]),
    'N6': ('D89', 'the cactus also beeps (the beep is the else branch)', [(SRC + 'commands.cpp',
           '        } else event(GameEventKind::Sfx, "move-blocked");\n    }\n    void naval_turn(',
           '        } event(GameEventKind::Sfx, "move-blocked");\n    }\n    void naval_turn(')]),
    'N7': ('D89', 'HP == damage no longer kills', [(SRC + 'turn.cpp',
           "    if (hp <= 0) { ch.status = 'D';", "    if (hp < 0) { ch.status = 'D';")]),
    'N8': ('D89', 'a dying active member stays selected', [(SRC + 'turn.cpp',
           ' if (g.party.active_character == i) g.party.active_character = 255; }', ' }')]),
    'N9': ('D89', 'the helper loops over the roster, not the party', [(SRC + 'turn.cpp',
           'for (int32_t i = 0; i < g.party.party_size && i < 6; ++i) {\n        if (i < g.party.character_count',
           'for (int32_t i = 0; i < g.party.character_count && i < 6; ++i) {\n        if (i < g.party.character_count')]),
    # ---------------------------------------------------------------- D-89 reference
    'T1': ('D89', 'reference: the naval OUCH rolls once on the active member again', [(TS + 'game.ts',
           '      partyRandomDamage(this.state, this.rand);\n      events.push({ kind: "party-changed" });\n',
           '      { const dmg = this.rand(1, 8); const active = this.state.characters[this.state.activeCharacter] ?? this.state.characters[0];\n'
           '        if (active) active.currentHp = Math.max(0, active.currentHp - dmg); }\n      events.push({ kind: "party-changed" });\n')]),
    'T2': ('D89', 'reference: the naval OUCH does not tell the party panel', [(TS + 'game.ts',
           '      partyRandomDamage(this.state, this.rand);\n      events.push({ kind: "party-changed" });\n',
           '      partyRandomDamage(this.state, this.rand);\n')]),
    'T3': ('D89', 'reference: the helper draws before it skips the dead', [(TS + 'world/survival.ts',
           '    if (state.characters[i]?.status === "D") continue;\n    applyDamage(state, i, rand(1, 8));',
           '    const d = rand(1, 8);\n    if (state.characters[i]?.status === "D") continue;\n    applyDamage(state, i, d);')]),
    'T4': ('D89', 'reference: the helper stops at five members', [(TS + 'world/survival.ts',
           'for (let i = 0; i < state.partySize && i < 6; i++) {\n    if (state.characters[i]?.status === "D") continue;',
           'for (let i = 0; i < state.partySize && i < 5; i++) {\n    if (state.characters[i]?.status === "D") continue;')]),
    'T5': ('D89', 'reference: HP == damage no longer kills', [(TS + 'world/survival.ts',
           '  ch.currentHp -= amount;\n  if (ch.currentHp <= 0) {', '  ch.currentHp -= amount;\n  if (ch.currentHp < 0) {')]),
    # ---------------------------------------------------------------- D-83 / D-84 native
    'N10': ('D83', 'the healer skips resurrect_apply (today\'s pre-fix code)', [(SRC + 'shops.cpp',
            "        resurrect_apply(c, g.karma);\n        c.current_hp = c.max_hp;",
            "        c.status = 'G';\n        c.current_hp = 1;")]),
    'N11': ('D83', 'the healer leaves HP at 1 after the routine', [(SRC + 'shops.cpp',
            "        resurrect_apply(c, g.karma);\n        c.current_hp = c.max_hp;",
            "        resurrect_apply(c, g.karma);")]),
    'N12': ('D83', 'the Refuge copies HP BEFORE the routine (the old maximum)', [(SRC + 'quest_world.cpp',
            'resurrect_apply(ch,g.karma);ch.current_hp=ch.max_hp;', 'ch.current_hp=ch.max_hp;resurrect_apply(ch,g.karma);')]),
    'N13': ('D83', 'the Refuge floors the karma BEFORE the routine', [(SRC + 'quest_world.cpp',
            'resurrect_apply(ch,g.karma);', 'resurrect_apply(ch,uint8_t(g.karma<75?75:g.karma));')]),
    'N14': ('D83', 'the threshold is karma < 99', [(SRC + 'magic.cpp', '    if (karma < 98)\n        p.exp', '    if (karma < 99)\n        p.exp')]),
    'N15': ('D83', 'the threshold is karma < 97', [(SRC + 'magic.cpp', '    if (karma < 98)\n        p.exp', '    if (karma < 97)\n        p.exp')]),
    'N16': ('D83', 'level and max HP only recomputed when the cut ran', [(SRC + 'magic.cpp',
            '    if (karma < 98)\n        p.exp = uint16_t(int(p.exp) * karma / 100);\n    p.level = 1;\n    for (int n = p.exp / 100; n > 0; n >>= 1)\n        ++p.level;\n    p.max_hp = uint16_t(30 * p.level);\n    return true;',
            '    if (karma < 98) {\n        p.exp = uint16_t(int(p.exp) * karma / 100);\n        p.level = 1;\n        for (int n = p.exp / 100; n > 0; n >>= 1)\n            ++p.level;\n        p.max_hp = uint16_t(30 * p.level);\n    }\n    return true;')]),
    'N17': ('D83', 'max HP is not recomputed', [(SRC + 'magic.cpp', '    p.max_hp = uint16_t(30 * p.level);\n    return true;\n}\nbool apply_target_spell', '    return true;\n}\nbool apply_target_spell')]),
    'N18': ('D83', 'the cut rounds instead of truncating', [(SRC + 'magic.cpp',
            'p.exp = uint16_t(int(p.exp) * karma / 100);', 'p.exp = uint16_t((int(p.exp) * karma + 50) / 100);')]),
    'N19': ('D83', "a bard's MP is INT, not INT >> 1", [(SRC + 'magic.cpp', 'p.current_mp = uint8_t(p.intelligence >> 1);', 'p.current_mp = p.intelligence;')]),
    'N20': ('D83', "the Refuge forces status 'G' on a member who was not dead", [(SRC + 'quest_world.cpp',
            'resurrect_apply(ch,g.karma);ch.current_hp=ch.max_hp;', "resurrect_apply(ch,g.karma);ch.current_hp=ch.max_hp;ch.status='G';")]),
    'N21': ('D83', 'the Refuge sets HP := max only for the members it revived', [(SRC + 'quest_world.cpp',
            'resurrect_apply(ch,g.karma);ch.current_hp=ch.max_hp;', 'if(resurrect_apply(ch,g.karma))ch.current_hp=ch.max_hp;')]),
    'N22': ('D83', 'the Refuge loops over the roster, not the party', [(SRC + 'quest_world.cpp',
            'for(int i=0;i<g.party.party_size && i<g.party.character_count;++i){auto &ch=g.party.characters[i];resurrect_apply',
            'for(int i=0;i<g.party.character_count;++i){auto &ch=g.party.characters[i];resurrect_apply')]),
    'N23': ('D83', 'the level loop is off by one', [(SRC + 'magic.cpp', 'for (int n = p.exp / 100; n > 0; n >>= 1)', 'for (int n = p.exp / 100; n > 1; n >>= 1)')]),
    # ---------------------------------------------------------------- D-83 / D-84 reference
    'T10': ('D83', 'reference: the healer skips resurrect_apply', [(TS + 'shops/shops.ts', '    applyResurrect(rec, state.karma);\n', '    rec.status = "G";\n')]),
    'T11': ('D83', 'reference: the healer leaves HP at 1', [(TS + 'shops/shops.ts', '    rec.currentHp = rec.maxHp; // 0x16f8', '    rec.currentHp = 1; // 0x16f8')]),
    'T12': ('D83', 'reference: the Refuge skips resurrect_apply', [(TS + 'world/blackthorn.ts',
            '      applyResurrect(c, state.karma); // BLCKTHRN', '      c.status = "G"; // BLCKTHRN')]),
    'T13': ('D83', 'reference: the Refuge floors the karma before the routine', [(TS + 'world/blackthorn.ts',
            'applyResurrect(c, state.karma); // BLCKTHRN', 'applyResurrect(c, Math.max(state.karma, REFUGE_KARMA_FLOOR)); // BLCKTHRN')]),
    'T14': ('D83', "reference: the Refuge forces status 'G' on a member who was not dead", [(TS + 'world/blackthorn.ts',
            '    c.currentHp = c.maxHp; // 0x0b98-0x0b9d', '    c.currentHp = c.maxHp; c.status = "G"; // 0x0b98-0x0b9d')]),
    'T15': ('D83', 'reference: the Refuge loops over the roster', [(TS + 'world/blackthorn.ts',
            'const n = state.partySize ?? state.characters.length;\n  let revived = 0;', 'const n = state.characters.length;\n  let revived = 0;')]),
    'T16': ('D83', 'reference: the threshold is karma < 99', [(TS + 'magic/cast.ts', '  if (karma < 98) {\n    target.exp', '  if (karma < 99) {\n    target.exp')]),
    'T17': ('D83', 'reference: the cut rounds', [(TS + 'magic/cast.ts', 'Math.floor((target.exp * karma) / 100)', 'Math.round((target.exp * karma) / 100)')]),
    # ---------------------------------------------------------------- D-85 native
    'N30': ('D85', 'the kill loop runs over the roster again', [(SRC + 'quest_world.cpp',
            'for(int i=0;i<g.party.party_size && i<g.party.character_count;++i){g.party.characters[i].current_hp=0;',
            'for(int i=0;i<g.party.character_count;++i){g.party.characters[i].current_hp=0;')]),
    'N31': ('D85', 'the kill loop overshoots the party by one', [(SRC + 'quest_world.cpp',
            'for(int i=0;i<g.party.party_size && i<g.party.character_count;++i){g.party.characters[i].current_hp=0;',
            'for(int i=0;i<=g.party.party_size && i<g.party.character_count;++i){g.party.characters[i].current_hp=0;')]),
    'N32': ('D85', 'the kill loop stops one member short', [(SRC + 'quest_world.cpp',
            'for(int i=0;i<g.party.party_size && i<g.party.character_count;++i){g.party.characters[i].current_hp=0;',
            'for(int i=0;i<g.party.party_size-1 && i<g.party.character_count;++i){g.party.characters[i].current_hp=0;')]),
    'N33': ('D85', 'an already dead member is skipped (the original rewrites it)', [(SRC + 'quest_world.cpp',
            '{g.party.characters[i].current_hp=0;g.party.characters[i].status=\'D\';}\n        event(sink,GameEventKind::MapChanged)',
            '{if(g.party.characters[i].status==\'D\')continue;g.party.characters[i].current_hp=0;g.party.characters[i].status=\'D\';}\n        event(sink,GameEventKind::MapChanged)')]),
    'N34': ('D85', 'the kill loop is capped at six', [(SRC + 'quest_world.cpp',
            'for(int i=0;i<g.party.party_size && i<g.party.character_count;++i){g.party.characters[i].current_hp=0;',
            'for(int i=0;i<g.party.party_size && i<6 && i<g.party.character_count;++i){g.party.characters[i].current_hp=0;')]),
    'N35': ('D85', "the device counts the roster's bursts again", [('native/targets/tdeck/main/alpha_runtime.cpp',
            'int32_t(game_.party.party_size)); // A4-PARITY2 D-85', 'int32_t(game_.party.character_count)); // A4-PARITY2 D-85')]),
    # ---------------------------------------------------------------- D-85 reference
    'T30': ('D85', 'reference: the wipe runs over the roster again', [(TS + 'game.ts',
            'for (const ch of this.state.characters.slice(0, this.state.partySize)) {\n      ch.currentHp = 0;', 'for (const ch of this.state.characters) {\n      ch.currentHp = 0;')]),
    'T31': ('D85', 'reference: the wipe overshoots the party by one', [(TS + 'game.ts',
            'this.state.characters.slice(0, this.state.partySize)) {\n      ch.currentHp = 0;', 'this.state.characters.slice(0, this.state.partySize + 1)) {\n      ch.currentHp = 0;')]),
    'T32': ('D85', 'reference: an already dead member is skipped', [(TS + 'game.ts',
            'this.state.characters.slice(0, this.state.partySize)) {\n      ch.currentHp = 0;', 'this.state.characters.slice(0, this.state.partySize)) {\n      if (ch.status === "D") continue;\n      ch.currentHp = 0;')]),
    'T33': ('D85', 'reference: the wipe is capped at six', [(TS + 'game.ts',
            'this.state.characters.slice(0, this.state.partySize)) {\n      ch.currentHp = 0;', 'this.state.characters.slice(0, Math.min(6, this.state.partySize))) {\n      ch.currentHp = 0;')]),
    # ---------------------------------------------------------------- D-86 native
    'N40': ('D86', 'the dungeon runs the stray outdoor turn again', [(SRC + 'commands.cpp',
            'if(!c.dungeon&&!g.position.map.location)r.turn();}', 'if(!g.position.map.location)r.turn();}')]),
    'N41': ('D86', 'no invalid digit passes a turn anywhere (the overworld loses its turn)', [(SRC + 'commands.cpp',
            'if(!c.dungeon&&!g.position.map.location)r.turn();}', ';}')]),
    'N42': ('D86', 'the town gains a turn for an invalid digit', [(SRC + 'commands.cpp',
            'if(!c.dungeon&&!g.position.map.location)r.turn();}', 'if(!c.dungeon)r.turn();}')]),
    'N43': ('D86', "a sleeping member is accepted as active", [(SRC + 'commands.cpp',
            "||party.characters[i].status=='D'||party.characters[i].status=='S'){r.message(\"Invalid!\")",
            "||party.characters[i].status=='D'){r.message(\"Invalid!\")")]),
    # ---------------------------------------------------------------- D-86 reference
    'T40': ('D86', 'reference: the dungeon runs the stray turn again', [(TS + 'game.ts',
            'if (this.state.position.location === 0 && !this.dungeonState) {', 'if (this.state.position.location === 0) {')]),
    'T41': ('D86', 'reference: no invalid digit passes a turn anywhere', [(TS + 'game.ts',
            'if (this.state.position.location === 0 && !this.dungeonState) {', 'if (false) {')]),
    'T42': ('D86', 'reference: the town gains a turn for an invalid digit', [(TS + 'game.ts',
            'if (this.state.position.location === 0 && !this.dungeonState) {', 'if (!this.dungeonState) {')]),
    # ---------------------------------------------------------------- D-87 native
    'N50': ('D87', 'a crop / plate adds food with no cap again', [(SRC + 'quest_search.cpp',
            'c.game.food=uint16_t(std::min<int32_t>(9999,int32_t(c.game.food)+1));', '++c.game.food;')]),
    'N51': ('D87', 'a value above the cap is left alone instead of clamped down', [(SRC + 'quest_search.cpp',
            'c.game.food=uint16_t(std::min<int32_t>(9999,int32_t(c.game.food)+1));', 'if(c.game.food<9999)++c.game.food;')]),
    'N52': ('D87', 'the cap is 10000', [(SRC + 'quest_search.cpp',
            'std::min<int32_t>(9999,int32_t(c.game.food)+1)', 'std::min<int32_t>(10000,int32_t(c.game.food)+1)')]),
    'N53': ('D87', 'the cap only applies at 9999 (not above)', [(SRC + 'quest_search.cpp',
            'c.game.food=uint16_t(std::min<int32_t>(9999,int32_t(c.game.food)+1));',
            'c.game.food=c.game.food>=9999&&c.game.food<10000?uint16_t(9999):uint16_t(c.game.food+1);')]),
    'N54': ('D87', 'the crop is refused at the cap (no tile change)', [(SRC + 'quest_search.cpp',
            'if(!s->volatile_tile)return {CommandStatus::InvalidContext};\n        const int next=tile==45?44',
            'if(!s->volatile_tile)return {CommandStatus::InvalidContext};\n        if(c.game.food>=9999)return {};\n        const int next=tile==45?44')]),
    'N55': ('D87', 'the karma is skipped when the food reaches the cap', [(SRC + 'quest_search.cpp',
            'c.game.karma=uint8_t(c.game.karma?c.game.karma-1:0);emit(sink,GameEventKind::Message,tile==45',
            'if(c.game.food<9999)c.game.karma=uint8_t(c.game.karma?c.game.karma-1:0);emit(sink,GameEventKind::Message,tile==45')]),
    # ---------------------------------------------------------------- D-87 reference
    'T50': ('D87', 'reference: the wheat branch adds food with no cap again', [(TS + 'game.ts',
            'this.setVolatileTerrain(nx, ny, PLOWED);\n      this.state.food = addWordCapped(this.state.food, 1); // A4-PARITY2 D-87: SJOG 0x1A50 / 0x1ABE counter_add(&food, 1, 9999)',
            'this.setVolatileTerrain(nx, ny, PLOWED);\n      this.state.food++;')]),
    'T51': ('D87', 'reference: the wheat branch leaves a value above the cap alone', [(TS + 'game.ts',
            'this.setVolatileTerrain(nx, ny, PLOWED);\n      this.state.food = addWordCapped(this.state.food, 1); // A4-PARITY2 D-87: SJOG 0x1A50 / 0x1ABE counter_add(&food, 1, 9999)',
            'this.setVolatileTerrain(nx, ny, PLOWED);\n      if (this.state.food < 9999) this.state.food++;')]),
    'T52': ('D87', 'reference: the plates add food with no cap again', [(TS + 'game.ts',
            'this.setVolatileTerrain(nx, ny, newTile);\n      this.state.food = addWordCapped(this.state.food, 1); // A4-PARITY2 D-87: SJOG 0x1A50 / 0x1ABE counter_add(&food, 1, 9999)',
            'this.setVolatileTerrain(nx, ny, newTile);\n      this.state.food++;')]),
    'T53': ('D87', 'reference: the plates cap at 10000', [(TS + 'game.ts',
            'this.setVolatileTerrain(nx, ny, newTile);\n      this.state.food = addWordCapped(this.state.food, 1); // A4-PARITY2 D-87: SJOG 0x1A50 / 0x1ABE counter_add(&food, 1, 9999)',
            'this.setVolatileTerrain(nx, ny, newTile);\n      this.state.food = addWordCapped(this.state.food, 1, 10000);')]),
    # ---------------------------------------------------------------- D-88 native
    'N60': ('D88', 'the clock ticks at the 9th activation', [(SRC + 'combat.cpp', 'if (++c.turn.combat_clock != 10)', 'if (++c.turn.combat_clock != 9)')]),
    'N61': ('D88', 'the clock ticks at the 11th activation', [(SRC + 'combat.cpp', 'if (++c.turn.combat_clock != 10)', 'if (++c.turn.combat_clock != 11)')]),
    'N62': ('D88', 'a modulo test instead of equality with a reset (a loaded 12 ticks at the 8th activation, not the 254th)', [(SRC + 'combat.cpp',
            'if (++c.turn.combat_clock != 10)', 'if (++c.turn.combat_clock % 10 != 0)')]),
    'N63': ('D88', 'two minutes per tick', [(SRC + 'combat.cpp', 'advance_clock(g, c.turn, 1, &arena_rand, nullptr, 0xFF);', 'advance_clock(g, c.turn, 2, &arena_rand, nullptr, 0xFF);')]),
    'N64': ('D88', 'the tick runs the housekeeping too (meals, starvation, poison in a fight)', [(SRC + 'combat.cpp',
            'advance_clock(g, c.turn, 1, &arena_rand, nullptr, 0xFF);', 'advance_clock(g, c.turn, 1, &arena_rand, nullptr, 0xFF);\n        turn_housekeeping(g, c.turn, arena_rand);')]),
    'N65': ('D88', 'the re-roll excludes the live location (g_location is not 0xFF in the arena)', [(SRC + 'combat.cpp',
            'advance_clock(g, c.turn, 1, &arena_rand, nullptr, 0xFF);', 'advance_clock(g, c.turn, 1, &arena_rand, nullptr);')]),
    'N66': ('D88', "the re-roll draws from the live stream, not the fight's", [(SRC + 'combat.cpp',
            'return static_cast<Engine *>(p)->rand(lo, hi); }};', 'return static_cast<Engine *>(p)->g.rng.next(lo, hi).value; }};')]),
    'N67': ('D88', 'only the player\'s activations are counted', [(SRC + 'combat.cpp', '            tick_combat_clock();\n', '            if (player(a)) tick_combat_clock();\n')]),
    'N68': ('D88', 'combat entry resets the counter (it is not combat-local)', [(SRC + 'combat.cpp',
            '    CombatActor *current() {\n        if (combat_over(s))', '    CombatActor *current() {\n        if (s.current < 0 && s.scan == 0 && s.action_count == 0) c.turn.combat_clock = 0;\n        if (combat_over(s))')]),
    'N69': ('D88', 'the .GAM byte +0x2DC is never written', [(SRC + 'persistence.cpp',
            '    put(b, 0x2dc, s.has("combatClock") ? s["combatClock"].integer() : 0);', '    (void)0;')]),
    'N70': ('D88', 'an absent key leaves the template byte (it must write 0)', [(SRC + 'persistence.cpp',
            '    put(b, 0x2dc, s.has("combatClock") ? s["combatClock"].integer() : 0);', '    if (s.has("combatClock")) put(b, 0x2dc, s["combatClock"].integer());')]),
    'N71': ('D88', 'a load does not read the counter back', [(SRC + 'persistence.cpp',
            'if (b[0x2dc])\n', 'if (false && b[0x2dc])\n')]),
    'N72': ('D88', 'the document never carries the counter (restore ignores it)', [(SRC + 'save_core.cpp',
            't.combat_clock = s.has("combatClock") ? uint8_t(s["combatClock"].integer()) : uint8_t(0);', 't.combat_clock = 0;')]),
    # ---------------------------------------------------------------- D-88 reference
    'T60': ('D88', 'reference: the clock ticks at the 9th activation', [(TS + 'combat/combat.ts', 'if (st.combatClock === 10) {', 'if (st.combatClock === 9) {')]),
    'T61': ('D88', 'reference: the counter has no 8-bit wrap', [(TS + 'combat/combat.ts',
            'st.combatClock = ((st.combatClock ?? 0) + 1) & 0xff;', 'st.combatClock = (st.combatClock ?? 0) + 1;')]),
    'T62': ('D88', 'reference: two minutes per tick', [(TS + 'combat/combat.ts', 'advanceClock(st, 1, (lo, hi) =>', 'advanceClock(st, 2, (lo, hi) =>')]),
    'T63': ('D88', 'reference: the re-roll excludes the live location', [(TS + 'combat/combat.ts',
            'this.crng.randRange(lo, hi), undefined, 0xff);', 'this.crng.randRange(lo, hi), undefined);')]),
    'T64': ('D88', 'reference: the counter is never written to SAVED.GAM', [(TS + 'saveNative.ts',
            '  if (state.combatClock !== undefined) gam[COMBAT_CLOCK_OFFSET] = state.combatClock & 0xff;\n', '')]),
    'T65': ('D88', 'reference: a load does not read the counter', [(TS + 'saveNative.ts',
            '  if (gam[COMBAT_CLOCK_OFFSET]) state.combatClock = gam[COMBAT_CLOCK_OFFSET]!;\n', '')]),
    'T66': ('D88', 'reference: the clock hook counts nothing without time-state guard (runs the tick before the count)', [(TS + 'combat/combat.ts',
            '    st.combatClock = ((st.combatClock ?? 0) + 1) & 0xff;\n    if (st.combatClock === 10) {', '    if (st.combatClock === 10) {')]),
}

ENV = dict(os.environ, PATH=BIN + os.pathsep + os.environ['PATH'])


def touch(path):
    now = time.time()
    os.utime(path, (now, now))


def run_native(build, group):
    g = GROUPS[group]
    r = subprocess.run([BIN + '/ninja.exe', '-C', build] + g['native_targets'], capture_output=True, text=True, env=ENV)
    if r.returncode:
        return 'INVALID', r.stdout[-800:]
    pattern = '^(' + '|'.join(g['native_tests']) + ')$'
    r = subprocess.run([BIN + '/ctest.exe', '-R', pattern], cwd=build, capture_output=True, text=True, env=ENV)
    failed = [l.strip() for l in r.stdout.splitlines() if '(Failed)' in l or 'Failed  ' in l]
    return ('KILLED' if r.returncode else 'SURVIVED'), ' | '.join(failed[:8])


def run_ts(build, group):
    g = GROUPS[group]
    failed = []
    r = subprocess.run(['npx.cmd', 'vitest', 'run'] + g['vitest'], cwd=os.path.join(ROOT, 'game'),
                       capture_output=True, text=True, errors='replace')
    if r.returncode:
        failed.append('vitest')
    for gen, args in g['generators']:
        r = subprocess.run([NODE, '--import', 'tsx', 'native/core/tools/' + gen] + args, cwd=ROOT,
                           capture_output=True, text=True, errors='replace')
        if r.returncode:
            failed.append(gen + ' ' + ' '.join(args))
    r = subprocess.run([BIN + '/ctest.exe', '-R', '^(gameplay_parity|quest_parity)$'], cwd=build,
                       capture_output=True, text=True, env=ENV)
    if r.returncode:
        failed += [l.strip() for l in r.stdout.splitlines() if '(Failed)' in l]
    return ('KILLED' if failed else 'SURVIVED'), ' | '.join(failed)


def apply(edits):
    saved = []
    for rel, old, new in edits:
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        o, n = (old.replace('\n', '\r\n'), new.replace('\n', '\r\n')) if '\r\n' in text else (old, new)
        assert text.count(o) == 1, (rel, text.count(o), old)
        saved.append((path, original))
        open(path, 'wb').write(text.replace(o, n, 1).encode('utf-8'))
        touch(path)
    return saved


def main():
    build = os.path.abspath(sys.argv[1])
    sel = sys.argv[2].split(',') if len(sys.argv) > 2 else None
    ids = [m for m, v in MUTANTS.items() if sel is None or m in sel or v[0] in sel]
    results = {}
    for mid in ids:
        group, what, edits = MUTANTS[mid]
        saved = []
        try:
            saved = apply(edits)
            verdict, detail = (run_ts if mid.startswith('T') else run_native)(build, group)
        finally:
            for path, original in saved:
                open(path, 'wb').write(original)
                touch(path)
        results[mid] = verdict
        print(f'{mid:4} {verdict:8} [{group}] {what}\n     {detail}', flush=True)
    clean = True
    for group in sorted({MUTANTS[m][0] for m in ids}):
        v1, d1 = run_native(build, group)
        v2, d2 = run_ts(build, group)
        print(f'restored tree [{group}]: native {"GREEN" if v1 == "SURVIVED" else v1} {d1}; reference {"GREEN" if v2 == "SURVIVED" else v2} {d2}')
        clean = clean and v1 == v2 == 'SURVIVED'
    killed = sum(v == 'KILLED' for v in results.values())
    print(f'\nA4-PARITY2 mutations: {killed}/{len(results)} killed')
    return 0 if killed == len(results) and clean else 1


if __name__ == '__main__':
    sys.exit(main())
