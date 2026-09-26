#!/usr/bin/env python
"""Alpha 3 A3-03 -- mutation validation of the remaining-SFX wiring.

Each mutation changes ONE production line, rebuilds, runs the A3-03 tests and
the A3-02 / A3-01 audio tests, and records which checks turn RED; the file is
then restored byte for byte and touched (a restored file is older than the
mutated object: without the touch ninja keeps the mutant). A mutation that
leaves every test GREEN is a SURVIVOR and fails this run.

Usage: python native/core/tools/a3_03_mutation_check.py <build-dir> <log> [M5,M12,...]
(the optional third argument re-runs only the named mutations)
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
CORE = os.path.join(ROOT, 'native', 'core')
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
AUDIO_PACK = os.path.join(ROOT, 'native/assets/openu5-audio.bin')
STOCK = os.path.join(CORE, 'fixtures/a3-01-audio-stock.bin')

TESTS = {
    'a3-03 inventory': [os.path.join(BUILD, 'a3_03_sfx_inventory.exe'), CORE],
    'a3-03 runtime': [os.path.join(BUILD, 'a3_03_sfx_runtime.exe'), RES, AUDIO_PACK],
    'a3-02 synth': [os.path.join(BUILD, 'a3_02_sfx_synth.exe'), os.path.join(CORE, 'fixtures/a3-02-sfx-reference.txt'), CORE],
    'a3-02 runtime': [os.path.join(BUILD, 'a3_02_sfx_runtime.exe'), RES, AUDIO_PACK],
    'a3-01 contract': [os.path.join(BUILD, 'a3_01_audio_contract.exe'), os.path.join(ROOT, 'game/src/core/sfx.ts'),
                       os.path.join(ROOT, 'game/src/ui/music.ts'), CORE, STOCK, AUDIO_PACK],
}
TARGETS = ['a3_03_sfx_inventory', 'a3_03_sfx_runtime', 'a3_02_sfx_synth', 'a3_02_sfx_runtime', 'a3_01_audio_contract']

SYN = 'native/core/src/sfx_synth.cpp'
INV = 'native/core/src/sfx_inventory.cpp'
AMB = 'native/core/src/ambient_sfx.cpp'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'

MUTATIONS = [
    # -- the brief's list ----------------------------------------------------------
    ('M1 the fountain cue is never emitted', AMB,
     '    case 3: cue = SfxId::AmbientFountain; break;  // 0x42c4, no phase gate',
     '    case 3: break;'),
    ('M2 the fountain loops every frame (ambient on every render, not per 55 ms tick)', RT,
     '    if(tick==ambient_tick_)return;\n    ambient_tick_=tick;', '    ambient_tick_=tick;'),
    ('M3 ambience preempts a scene / gameplay cue', SYN,
     '        if (voice_.active() || pending_count_) {\n            ++stats_.ambient_skipped;',
     '        if (false) {\n            ++stats_.ambient_skipped;'),
    ('M4 the troll (arena) victory cue is omitted', INV,
     '    if (std::strcmp(text, "VICTORY!") == 0) return SfxId::VictoryFanfare;             // COMBAT 0x0d02\n', ''),
    ('M5 the victory cue fires twice (the Ended line sounds too)', INV,
     '    if (!text || ended) return SfxId::None;', '    (void)ended;\n    if (!text) return SfxId::None;'),
    ('M6 the quake sound drives its presentation (the shake stretches with submitted audio)', RT,
     '        else{quake_start_us_=now;quake_pulses_=openu5::kQuakePulses;}',
     '        else{quake_start_us_=now;quake_pulses_=openu5::kQuakePulses+int(audio_.stats().sfx_submitted%3);}'),
    ('M7 ritual audio controls the scene pacing (the Refuge pacer shifted by submitted audio)', RT,
     '    narrative_pacer_.pump(uint32_t(esp_timer_get_time()/1000),{this,narrative_beat},{this,release_scene_event});',
     '    narrative_pacer_.pump(uint32_t(esp_timer_get_time()/1000-40*audio_.stats().sfx_submitted),{this,narrative_beat},{this,release_scene_event});'),
    ('M8 a load leaves the ambience state running', RT, '    audio_.flush_for_load();\n    reset_ambient();',
     '    audio_.flush_for_load();'),
    ('M9 an unsupported SfxId is silently accepted (bard-song)', SYN,
     '    case SfxId::TrapdoorFall: case SfxId::SearchFail:',
     '    case SfxId::TrapdoorFall: case SfxId::SearchFail: case SfxId::BardSong:'),
    ('M10 the runtime drops one supported cue (moongate)', RT,
     '    if(id==openu5::SfxId::Quake)return;', '    if(id==openu5::SfxId::Quake||id==openu5::SfxId::Moongate)return;'),
    # -- further guards ---------------------------------------------------------------
    ('M11 the shake does not carry the rumble (one quake per cue, not per shake)', RT,
     '    if(e.kind==openu5::GameEventKind::Quake){audio_.play_sfx(openu5::SfxId::Quake);return;}',
     '    if(e.kind==openu5::GameEventKind::Quake)return;'),
    ('M12 every repeat coalesces again (the A3-02 rule)', SYN,
     '    if (sfx_coalesces(r.id) && pending_count_) {', '    if (sfx_class(r.id) != SfxClass::Instrument && pending_count_) {'),
    ('M13 ambience in a dungeon (the 0x21-0x7f rule dropped)', RT,
     '    if(!arena&&context_.dungeon&&dungeon_.active)return;\n', ''),
    ('M14 the ladder legs both rise (stride sign)', SYN,
     '    b.add(speaker_sweep_loop(inc, 1, count, kLadderHigh, 0, kLadderCalls, int16_t(-kLadderStride)));',
     '    b.add(speaker_sweep_loop(inc, 1, count, kLadderHigh, 0, kLadderCalls, kLadderStride));'),
    ('M15 the fanfare\'s last note at the first pitch', SYN,
     '        b.add(speaker_sweep(0x17d4, 1, 0x5460, 0x12c, 3));                            // 0x4391-0x43a5',
     '        b.add(speaker_sweep(0x11f8, 1, 0x5460, 0x12c, 3));                            // 0x4391-0x43a5'),
    ('M16 the trapdoor ramp everywhere (location 29 test dropped)', RT,
     'std::strcmp(e.text,"A TRAPDOOR!")==0&&game_.position.map.location==29)',
     'std::strcmp(e.text,"A TRAPDOOR!")==0)'),
    ('M17 the clock is never re-armed', RT,
     '    else if(key!=ambient_clock_key_){ambient_clock_key_=key;ambient_.rearm(uint8_t(t.hour));}',
     '    else if(key!=ambient_clock_key_){ambient_clock_key_=key;}'),
    ('M18 the scan keeps the LAST of equal distances', AMB,
     '            if (d >= best) continue; // 0x41b8 jge: only strictly nearer',
     '            if (d > best) continue; // 0x41b8 jge: only strictly nearer'),
    ('M19 the revival plays one tone whatever the party', RT,
     'self.audio_.play_sfx(openu5::SfxId::RefugeRevival,int32_t(self.game_.party.character_count));',
     'self.audio_.play_sfx(openu5::SfxId::RefugeRevival,1);'),
    ('M20 the escape glide is omitted', INV,
     '    if (std::strcmp(text, "Escape!") == 0) return SfxId::CombatEscape;                 // SJOG 0x1c37 / CMDS 0x18ac\n', ''),
    ('M21 the healer jingle on any shop result', RT,
     '       e.shop->session&&e.shop->session->type==openu5::ShopType::Healer&&e.shop->result->ok&&',
     '       e.shop->session&&e.shop->result->ok&&'),
    ('M22 the rumble is a flat tone (no redraw per half-cycle)', SYN,
     '            rumble_half_ = uint64_t(pit_divisor(speaker_rumble_draw(*rumble_, s.value, s.start))) * kFineRate;\n        }',
     '        }'),
    ('M23 the chimes do not run down outside the clock branch', AMB,
     '    if (chimes_ && (phase_ == 0 || phase_ == 4)) --chimes_; // 0x430e-0x4323, whatever the class',
     '    if (nearest_class == 1 && chimes_ && (phase_ == 0 || phase_ == 4)) --chimes_;'),
    ('M24 the attract demo\'s thunder is not routed', RT,
     '                if(intro_frame_.thunder)audio_.play_sfx(openu5::SfxId::IntroThunder);\n', ''),
    ('M25 the healer jingle on any healer result (the service test dropped)', RT,
     'e.shop->result->ok&&\n       e.shop->result->message&&std::strcmp(e.shop->result->message,"It is done.")==0)',
     'e.shop->result->ok)'),
    ('M26 no ambience in an arena (the arena treated as a dungeon corridor)', RT,
     '    const bool arena=context_.combat&&combat_.initialized;', '    const bool arena=false;'),
]


def run(cmd, cwd):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    return run([CMAKE, '--build', BUILD, '--target'] + TARGETS, ROOT)


def run_tests():
    results = {}
    for name, cmd in TESTS.items():
        code, out = run(cmd, ROOT)
        reds = [l for l in out.splitlines() if l.startswith('RED')]
        results[name] = (code, reds)
    return results


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-03 mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
    survivors = 0
    only = set(sys.argv[3].split(',')) if len(sys.argv) > 3 else None
    selected = [m for m in MUTATIONS if not only or m[0].split()[0] in only]
    for label, rel, old, new in selected:
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        eol = '\r\n' if '\r\n' in text else '\n'
        o, n = old.replace('\n', eol), new.replace('\n', eol)
        count = text.count(o)
        if count != 1:
            lines.append('%s: ANCHOR NOT UNIQUE (%d) in %s' % (label, count, rel))
            survivors += 1
            continue
        try:
            open(path, 'wb').write(text.replace(o, n).encode('utf-8'))
            code, out = build()
            if code != 0:
                lines.append('%s [%s]: BUILD FAILED (counts as killed)' % (label, rel))
                lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                continue
            results = run_tests()
        finally:
            open(path, 'wb').write(original)
            os.utime(path, None)
        killed = any(code != 0 for code, _ in results.values())
        survivors += 0 if killed else 1
        lines.append('%s [%s]: %s' % (label, rel, 'KILLED' if killed else 'SURVIVED'))
        for name, (code, reds) in results.items():
            if code != 0:
                lines.append('    %-15s exit=%d  %d RED' % (name, code, len(reds)))
                lines += ['        ' + r for r in reds[:6]]
    code, _ = build()
    restored = run_tests()
    lines.append('')
    lines.append('restored build exit=%d; ' % code + ', '.join('%s exit=%d' % (k, v[0]) for k, v in restored.items()))
    lines.append('mutations: %d, killed: %d, survived: %d' % (len(selected), len(selected) - survivors, survivors))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines[-3:]))
    sys.exit(1 if survivors or code or any(v[0] for v in restored.values()) else 0)


if __name__ == '__main__':
    main()
