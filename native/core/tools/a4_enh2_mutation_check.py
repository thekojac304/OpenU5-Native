"""Alpha 4 A4-ENH2 -- mutation check of the extended difficulty and cheats (ALPHA4_UI.md section 11).

    python tools/a4_enh2_mutation_check.py <build-dir> [ids,comma,separated] [--anchors]

As tools/a4_enh1_mutation_check.py: each mutant is one edit of production code
(the anchor matched in either line ending -- the checkout mixes LF and CRLF,
sometimes within one file); the driver applies it, touches the file, builds
the named test targets, runs them, and restores + touches the file whatever
happens. KILLED = a named test exits non-zero, INVALID = it does not build.
--anchors only checks that every anchor matches once. The host toolchain is
put on PATH by the driver itself (absolute ninja, A4-PARITY1's form).

The defect classes the brief names: a non-identity Original row (the A4-ENH1
and A4-ENH2 preservation goldens must go RED), Easy's 120 % / 65 %, Custom's
values and persistence, the cheat-over-difficulty precedence, the save
whitelist's new fields, the dungeon and starvation hooks (including one that
moves an RNG draw), every new cheat and its guards, the World cheats' sites,
and the menu and runtime wiring.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
BIN = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin'
NINJA = BIN + '/ninja.exe'
ENV = dict(os.environ, PATH=BIN + os.pathsep + os.environ.get('PATH', ''))
PACK = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
GAM = os.path.join(ROOT, 'game/assets/init.gam')
ENH_H = 'native/core/include/openu5/enhanced.h'
ENH = 'native/core/src/enhanced.cpp'
MENU = 'native/core/src/system_menu.cpp'
SAVE = 'native/core/src/save_core.cpp'
TURN = 'native/core/src/turn.cpp'
DUNGEON = 'native/core/src/dungeon.cpp'
OUTDOOR = 'native/core/src/outdoor.cpp'
REST = 'native/core/src/rest.cpp'
SHOPS = 'native/core/src/shops.cpp'
COMMANDS = 'native/core/src/commands.cpp'
COMBAT = 'native/core/src/combat.cpp'
RUNTIME = 'native/targets/tdeck/main/alpha_runtime.cpp'
TESTS = {
    'rules2': ('a4_enh2_rules', [GAM]),
    'rt2': ('a4_enh2_runtime', [PACK]),
    'gold2': ('a4_enh2_preservation', []),
    'gold2rt': ('a4_enh2_preservation_runtime', [PACK]),
    'rules1': ('a4_enh1_rules', [GAM]),
    'rt1': ('a4_enh1_runtime', [PACK]),
    'gold1': ('a4_enh1_preservation', []),
    'gold1rt': ('a4_enh1_preservation_runtime', [PACK]),
}
GOLD = ['gold1', 'gold1rt', 'gold2', 'gold2rt']

# (id, what, file, anchor, replacement, tests)
MUTANTS = [
    # --- Original stays the 1988 game ---------------------------------------
    ('O1', 'Original enemy damage 99 % (non-identity Original row)', ENH_H,
     'inline constexpr GameplayRules kOriginalRules{100, 100, 100, 100, 1, 100, 100, 100};',
     'inline constexpr GameplayRules kOriginalRules{99, 100, 100, 100, 1, 100, 100, 100};', GOLD + ['rules1', 'rules2']),
    ('O2', 'Original player damage 99 %', ENH_H,
     'inline constexpr GameplayRules kOriginalRules{100, 100, 100, 100, 1, 100, 100, 100};',
     'inline constexpr GameplayRules kOriginalRules{100, 99, 100, 100, 1, 100, 100, 100};', GOLD + ['rules1', 'rules2']),
    ('O3', 'Original poison every 2nd turn', ENH_H,
     'inline constexpr GameplayRules kOriginalRules{100, 100, 100, 100, 1, 100, 100, 100};',
     'inline constexpr GameplayRules kOriginalRules{100, 100, 100, 100, 2, 100, 100, 100};', GOLD + ['rules1', 'rules2']),
    ('O4', 'Original dungeon wanderers 90 %', ENH_H,
     'inline constexpr GameplayRules kOriginalRules{100, 100, 100, 100, 1, 100, 100, 100};',
     'inline constexpr GameplayRules kOriginalRules{100, 100, 100, 100, 1, 100, 90, 100};', GOLD + ['rules2']),
    ('O5', 'Original starvation 50 %', ENH_H,
     'inline constexpr GameplayRules kOriginalRules{100, 100, 100, 100, 1, 100, 100, 100};',
     'inline constexpr GameplayRules kOriginalRules{100, 100, 100, 100, 1, 100, 100, 50};', GOLD + ['rules2']),
    ('O6', 'every preset resolves to the Custom values (precedence: difficulty ignored)', ENH,
     '    const uint16_t v = (e.difficulty == Difficulty::Custom ? e.custom : gameplay_rules(e.difficulty)).*field;',
     '    const uint16_t v = e.custom.*field;', ['rules2', 'rt2', 'rules1']),
    ('O7', 'Custom resolves to Original\'s row (its values ignored)', ENH,
     '    const uint16_t v = (e.difficulty == Difficulty::Custom ? e.custom : gameplay_rules(e.difficulty)).*field;',
     '    const uint16_t v = gameplay_rules(e.difficulty).*field;', ['rules2', 'rt2']),
    # --- the presets --------------------------------------------------------
    ('E1', 'Easy outgoing damage back to 100 %', ENH,
     '    {65, 120, 200, 65, 10, 50, 65, 25},   // Easy', '    {65, 100, 200, 65, 10, 50, 65, 25},   // Easy',
     ['rules2', 'rules1']),
    ('E2', 'Easy overworld encounters back to 75 %', ENH,
     '    {65, 120, 200, 65, 10, 50, 65, 25},   // Easy', '    {65, 120, 200, 75, 10, 50, 65, 25},   // Easy',
     ['rules2', 'rules1']),
    ('E3', 'Easy dungeon wanderers 100 %', ENH,
     '    {65, 120, 200, 65, 10, 50, 65, 25},   // Easy', '    {65, 120, 200, 65, 10, 50, 100, 25},   // Easy',
     ['rules2', 'rt2']),
    ('E4', 'Relaxed starvation at Original', ENH,
     '    {85, 100, 150, 90, 4, 75, 90, 50},    // Relaxed', '    {85, 100, 150, 90, 4, 75, 90, 100},    // Relaxed',
     ['rules2']),
    ('E5', 'the outgoing hook removed from combat damage()', COMBAT,
     '                d = rules_outgoing_damage(g, d);', '                (void)0;', ['rules2']),
    # --- Custom: persistence and the page -----------------------------------
    ('C1', 'the "custom" array is never written', SAVE,
     '        if (!same_rules(g.enhanced.custom, kOriginalRules)) {', '        if (false) {', ['rules2', 'rt2']),
    ('C2', 'the "custom" array is written even at Original\'s values', SAVE,
     '        if (!same_rules(g.enhanced.custom, kOriginalRules)) {', '        if (true) {', ['rules2']),
    ('C3', 'a load ignores the "custom" array', SAVE,
     '                g.enhanced.custom.*kRuleChoices[i].field = uint16_t(v.integer());', '                (void)v;',
     ['rules2', 'rt2']),
    ('C4', 'a load takes a foreign value (not one of the choices)', SAVE,
     '            if (fits(v, 0, UINT16_MAX) && rule_choice(RuleField(i), uint16_t(v.integer())) >= 0)',
     '            if (fits(v, 0, UINT16_MAX))', ['rules2']),
    ('C5', 'a load reads an object as the "custom" array', SAVE,
     '        for (unsigned i = 0; e["custom"].kind == J::Array && i < unsigned(RuleField::Count); ++i) {',
     '        for (unsigned i = 0; i < unsigned(RuleField::Count); ++i) {', ['rules2']),
    ('C6', 'enhanced_is_default ignores the Custom values (changed values lost on an Original save)', ENH,
     '           same_rules(e.custom, kOriginalRules);', '           true;', ['rules2']),
    ('C7', '"custom" is not a difficulty name the save knows', SAVE,
     'constexpr const char *kDifficultyKeys[] = {"original", "relaxed", "easy", "custom"};',
     'constexpr const char *kDifficultyKeys[] = {"original", "relaxed", "easy", "custom2"};', ['rules2', 'rt2']),
    ('C8', 'left / right on the Custom page changes nothing it sends', MENU,
     '  pending_.kind=SystemMenuIntentKind::SetDifficulty;pending_.difficulty=Difficulty::Custom;pending_.custom=enhanced_.custom;}',
     '  }', ['rt2']),
    ('C9', 'the runtime drops the Custom values of a SetDifficulty', RUNTIME,
     'game_.enhanced.custom=intent.custom;', '', ['rt2']),
    ('C10', 'Back from the Custom page returns to the root', MENU,
     'if(page_==Page::Custom||page_==Page::CheatGroup){const bool c=page_==Page::Custom;',
     'if(page_==Page::CheatGroup){const bool c=page_==Page::Custom;', ['rt2']),
    ('C11', 'a Custom value wraps around instead of holding at the end', ENH,
     '    return c.values[std::clamp(i + (dir < 0 ? -1 : 1), 0, c.count - 1)];',
     '    return c.values[(i + (dir < 0 ? c.count - 1 : 1)) % c.count];', ['rules2', 'rt2']),
    # --- dungeon wanderers and starvation -----------------------------------
    ('D1', 'the wanderer re-arm hook removed (every rolled wanderer placed)', DUNGEON,
     '        if (!rules_wanderer_allowed(g, d.pos.floor)) make_dormant(w);', '', ['rules2', 'rt2']),
    ('D2', 'the decision moved before the hidden roll: a refused re-arm skips a 1988 draw', DUNGEON,
     '        if ((w.type == 0x16 || w.type == 0x18) && g.rng.next(0, 99).value > 0x30)\n            w.hidden = true;\n        // A4-ENH2: the difficulty\'s share of the re-arms that place it, asked\n        // after every 1988 draw above (all of them on Original).\n        if (!rules_wanderer_allowed(g, d.pos.floor)) make_dormant(w);',
     '        if (!rules_wanderer_allowed(g, d.pos.floor)) { make_dormant(w); return; }\n        if ((w.type == 0x16 || w.type == 0x18) && g.rng.next(0, 99).value > 0x30)\n            w.hidden = true;',
     ['rules2']),
    ('D3', 'starvation severity not passed to the starvation site', TURN,
     'party_random_damage(g,rand,true); }', 'party_random_damage(g,rand); }', ['rules2', 'rt2']),
    ('D4', 'every party_random_damage caller scaled (fire, quake, cactus)', TURN,
     '        if (const int32_t v = starvation ? rules_starvation_damage(g,d) : d) apply_damage(g, i, v);',
     '        (void)starvation;\n        if (const int32_t v = rules_starvation_damage(g,d)) apply_damage(g, i, v);', ['rules2']),
    ('D5', '"Starving!" said even when starvation is off', TURN,
     'if (rules_starvation_damage(g,1)) message(out,TurnMessage::Starving);', 'message(out,TurnMessage::Starving);',
     ['rules2', 'rt2']),
    ('D6', 'starvation Off scales to 1 (scale()\'s floor) instead of 0', ENH,
     '    return p ? scale(d, p) : 0;', '    return scale(d, p ? p : 1);', ['rules2', 'rt2']),
    # --- precedence: the World cheats over every difficulty -----------------
    ('P1', 'No Hunger stops meals but not starvation', ENH,
     '(field == &R::hunger_pct || field == &R::starvation_pct)) return 0;', '(field == &R::hunger_pct)) return 0;',
     ['rules2']),
    ('P2', 'Disable Random Encounters spares the dungeon', ENH,
     '        (field == &R::encounter_pct || field == &R::dungeon_encounter_pct))', '        (field == &R::encounter_pct))',
     ['rules2']),
    ('P3', 'No Poison Damage overridden by the difficulty (a tick every turn)', ENH,
     "    if ((e.toggles & cheat_bit(CheatKind::NoPoisonDamage)) && field == &R::poison_interval) return 0;",
     "    if ((e.toggles & cheat_bit(CheatKind::NoPoisonDamage)) && field == &R::poison_interval) return 1;",
     ['rules2', 'rt2']),
    ('P4', 'roaming monsters not cleared under Disable Random Encounters', OUTDOOR,
     'return random_encounters_disabled(g)||std::max(', 'return std::max(', ['rt2']),
    ('P5', 'a placed wanderer kept under Disable Random Encounters', DUNGEON,
     '        if (d.wanderer.type != 255 && random_encounters_disabled(g)) make_dormant(d.wanderer);', '', ['rules2']),
    ('P6', 'the camp still ambushed under Disable Random Encounters', REST,
     '            if (!random_encounters_disabled(c.game)) {', '            if (true) {', ['rules2']),
    ('P7', 'the inn\'s poisoned sleeper still dies under No Poison Damage / God Mode', SHOPS,
     '            if (!poison_harmless(g)) { // A4-ENH2', '            if (true) { // A4-ENH2', ['rules2']),
    ('P8', 'the toggles are not saved', SAVE, 'if (g.enhanced.toggles) e["toggles"]', 'if (false) e["toggles"]',
     ['rules2', 'rt2']),
    ('P9', 'a load keeps a stray toggle bit', SAVE,
     'g.enhanced.toggles = uint32_t(e["toggles"].integer()) & kToggleCheats;',
     'g.enhanced.toggles = uint32_t(e["toggles"].integer());', ['rules2']),
    ('P10', 'enhanced_is_default ignores the toggles', ENH, '!e.cheats_used && !e.toggles &&', '!e.cheats_used &&',
     ['rules2']),
    # --- the new cheats -----------------------------------------------------
    ('X1', 'Restore MP lowers a value above the class maximum', ENH,
     "            if (c.status == 'D' || mp < 0 || c.current_mp >= mp) continue;",
     "            if (c.status == 'D' || mp < 0 || c.current_mp == mp) continue;", ['rules2']),
    ('X2', 'Revive Party allowed in combat', ENH,
     '    case CheatKind::ReviveParty: {\n        if (in_combat) {', '    case CheatKind::ReviveParty: {\n        if (in_combat && false) {',
     ['rules2', 'rt2']),
    ('X3', 'Revive Party touches the living', ENH, "            if (c.status != 'D') continue;",
     "            if (c.status == 'G') continue;", ['rules2']),
    ('X4', 'the Max cheats lower a value already above the cap', ENH, '        if (before >= cap) {', '        if (before == cap) {',
     ['rules2']),
    ('X5', 'keys, torches and gems capped at 100', ENH_H, 'constexpr int32_t kFoodCap = 9999, kCounterCap = 99;',
     'constexpr int32_t kFoodCap = 9999, kCounterCap = 100;', ['rules2', 'rt2']),
    ('X6', 'Give Reagents also fills the skull keys (a quest item)', ENH,
     '        std::snprintf(r.text, sizeof(r.text), "Reagents: %d each", int(kCounterCap));',
     '        g.skull_keys = kCounterCap;\n        std::snprintf(r.text, sizeof(r.text), "Reagents: %d each", int(kCounterCap));',
     ['rules2']),
    ('X7', 'God Mode misses the naval OUCH again', COMMANDS,
     'if (i < c.game.party.character_count && !party_damage_blocked(c.game)) {', 'if (i < c.game.party.character_count) {',
     ['rules2']),
    ('X8', 'a group row applies the cheat of its index, not its own', MENU,
     'pending_.kind=SystemMenuIntentKind::Cheat;pending_.cheat=k;', 'pending_.kind=SystemMenuIntentKind::Cheat;pending_.cheat=CheatKind(cursor_);',
     ['rt2', 'rt1']),
    ('X9', 'a World row shows God Mode\'s state', MENU, 'cheat_on(enhanced_,k)?"On":"Off"', 'enhanced_.god_mode?"On":"Off"',
     ['rt2']),
    ('X10', 'Revive Party lowers MP above the class value (the review\'s finding)', ENH,
     'mp >= 0 && c.current_mp < mp) c.current_mp = uint8_t(mp);', 'mp >= 0) c.current_mp = uint8_t(mp);', ['rules2']),
]


def touch(p):
    t = time.time()
    os.utime(p, (t, t))


def build(build_dir, targets):
    r = subprocess.run([NINJA, '-C', build_dir] + targets, capture_output=True, text=True, env=ENV)
    return r.returncode == 0, r.stdout[-1500:]


def run(build_dir, key):
    target, args = TESTS[key]
    r = subprocess.run([os.path.join(build_dir, target + '.exe')] + args, capture_output=True, text=True,
                       timeout=900, cwd=ROOT, env=ENV)
    reds = []
    for l in r.stdout.splitlines():
        w = l.split()
        if w and w[0] == 'RED' and len(w) > 1:
            reds.append(w[1])
    return r.returncode, reds


def main():
    build_dir = os.path.abspath(sys.argv[1])
    args = [a for a in sys.argv[2:] if not a.startswith('--')]
    only = set(args[0].split(',')) if args else None
    anchors_only = '--anchors' in sys.argv
    killed = invalid = survived = 0
    for mid, what, rel, anchor, repl, keys in MUTANTS:
        if only and mid not in only:
            continue
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        found = None
        for eol in ('\r\n', '\n'):
            a = anchor.replace('\r\n', '\n').replace('\n', eol)
            if text.count(a) == 1:
                found = (a, repl.replace('\r\n', '\n').replace('\n', eol))
                break
        if not found or found[0] == found[1]:
            print(f'{mid} ANCHOR-MISSING or no-op -- {what}', flush=True)
            invalid += 1
            continue
        if anchors_only:
            print(f'{mid} anchor ok -- {what}', flush=True)
            continue
        try:
            open(path, 'wb').write(text.replace(found[0], found[1]).encode('utf-8'))
            touch(path)
            targets = [TESTS[k][0] for k in keys]
            ok, out = build(build_dir, targets)
            if not ok:
                print(f'{mid} INVALID (does not build) -- {what}\n{out}', flush=True)
                invalid += 1
                continue
            results = [(k,) + run(build_dir, k) for k in keys]
            dead = any(code != 0 for _, code, _ in results)
            detail = '; '.join(f'{TESTS[k][0]} exit={code} RED={",".join(r) or "-"}' for k, code, r in results)
            print(f'{mid} {"KILLED" if dead else "SURVIVED"} -- {what} -- {detail}', flush=True)
            killed += dead
            survived += not dead
        finally:
            open(path, 'wb').write(original)
            touch(path)
    if anchors_only:
        return
    ok, _ = build(build_dir, [t for t, _ in TESTS.values()])
    green = ok and all(run(build_dir, k)[0] == 0 for k in TESTS)
    print(f'A4-ENH2 mutation check: {killed} killed, {survived} survived, {invalid} invalid; '
          f'restored build {"GREEN" if green else "RED"}')


if __name__ == '__main__':
    main()
