#!/usr/bin/env python
"""Alpha 3 A3-04G -- mutation validation of the save-inspect / storage-heap
change (ALPHA3_AUDIO.md section 28).

Each mutation changes ONE production anchor -- the storage shell
(alpha_save.cpp), the generation gate (alpha_save_generation.cpp) or the
runtime's in-menu Save (alpha_runtime.cpp) -- rebuilds a3_04g_storage_runtime
(the REAL alpha_save.cpp over the fake SD card, with the heap census) and
records which checks turn RED. The file is restored byte for byte and touched
(a restored file is older than the mutated object: without the touch ninja
keeps the mutant). A mutation that leaves every check GREEN is a SURVIVOR and
fails this run; one that does not build is INVALID (not killed).

Usage: python native/core/tools/a3_04g_mutation_check.py <build-dir> <log> [M1,M2,...]
(the optional third argument re-runs only the named mutations)
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
TARGET = 'a3_04g_storage_runtime'

SAVE = 'native/targets/tdeck/main/alpha_save.cpp'
GEN = 'native/targets/tdeck/main/alpha_save_generation.cpp'
RT = 'native/targets/tdeck/main/alpha_runtime.cpp'

MUTATIONS = [
    # --- retention ---
    ('M1 inspect keeps each slot\'s files', SAVE,
     '        release_candidate(v);\n        release_stage(s.stage);\n    }',
     '        release_stage(s.stage);\n    }'),
    ('M2 inspect keeps each slot until the end', SAVE,
     '        release_candidate(v);\n        release_stage(s.stage);\n    }',
     '        release_candidate(v);\n    }'),
    ('M3 load keeps a staged slot while reading the other', SAVE,
     'for(int i=0;i<2;++i){candidate(i,s.candidates[i],s);release_stage(s.stage);}',
     'for(int i=0;i<2;++i){candidate(i,s.candidates[i],s);}'),
    ('M4 save keeps its workspace', SAVE,
     'auto &s=*workspace;\n    ReleaseOnExit release{s};',
     'auto &s=*workspace;'),
    ('M5 release_stage leaves the document', GEN,
     's.document=openu5::save::Json{};s.outdoor=',
     's.outdoor='),
    ('M6 verify empties the stage after the import (the pre-A3-04G order)', GEN,
     '    s.game={};s.turn={};s.document={};s.commands={};s.outdoor={};s.terrain={};s.actors={};\n    v.side.assign(',
     '    s.game={};s.turn={};s.commands={};s.outdoor={};s.terrain={};s.actors={};\n    v.side.assign('),
    # --- the save list ---
    ('M7 a cached summary is used whatever the commit says', SAVE,
     'return known[i]&&k.magic==c.magic&&k.version==c.version&&k.sequence==c.sequence&&k.gam==c.gam&&k.ool==c.ool&&k.json==c.json;',
     'return known[i]&&k.magic==c.magic;'),
    ('M8 the commit comparison ignores the sequence', SAVE,
     '&&k.sequence==c.sequence&&',
     '&&'),
    ('M9 nothing is ever reused', SAVE,
     'if(s.cache.hit(i,commit)){slots[i]=s.cache.summary[i];source[i]="cached";continue;}',
     'if(false&&s.cache.hit(i,commit)){slots[i]=s.cache.summary[i];source[i]="cached";continue;}'),
    ('M10 a refused generation is cached too', SAVE,
     'else{slots[i].present=true;source[i]="refused";}',
     'else{slots[i].present=true;source[i]="refused";s.cache.store(i,v.commit,slots[i]);}'),
    ('M11 a failed save does not forget its slot', SAVE,
     'if(ok)s.cache.store(slot,s.verified.commit,summarize_candidate(s.verified,s.stage));else s.cache.forget(slot);',
     'if(ok)s.cache.store(slot,s.verified.commit,summarize_candidate(s.verified,s.stage));'),
    ('M12 a good save does not store its slot', SAVE,
     'if(ok)s.cache.store(slot,s.verified.commit,summarize_candidate(s.verified,s.stage));else s.cache.forget(slot);',
     'if(!ok)s.cache.forget(slot);'),
    ('M13 a good save stores the other slot', SAVE,
     'if(ok)s.cache.store(slot,s.verified.commit,summarize_candidate(s.verified,s.stage));else s.cache.forget(slot);',
     'if(ok)s.cache.store(1-slot,s.verified.commit,summarize_candidate(s.verified,s.stage));else s.cache.forget(slot);'),
    ('M14 the summary names the wrong sequence', GEN,
     'out.present=out.valid=true;out.sequence=v.commit.sequence;',
     'out.present=out.valid=true;out.sequence=v.commit.sequence+1;'),
    ('M15 the summary drops the name', GEN,
     'if(s.game.party.character_count)std::snprintf(out.name',
     'if(false&&s.game.party.character_count)std::snprintf(out.name'),
    ('M16 a present slot that fails to read is listed empty', SAVE,
     'else{slots[i].present=true;source[i]="refused";}',
     'else{source[i]="refused";}'),
    # --- the in-menu Save ---
    ('M17 the menu keeps its list after an in-menu Save', RT,
     'save_.inspect(slots);system_menu_.set_save_slots(slots);}',
     '(void)slots;}'),
]


def build():
    env = dict(os.environ)
    env['PATH'] = os.path.dirname(CMAKE) + os.pathsep + env.get('PATH', '')
    r = subprocess.run([CMAKE, '--build', BUILD, '--target', TARGET], env=env, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def test():
    exe = os.path.join(BUILD, TARGET + '.exe')
    r = subprocess.run([exe, RES], capture_output=True, text=True, errors='replace')
    reds = [l.strip() for l in r.stdout.splitlines() if l.strip().startswith('RED ')]
    return [(TARGET, r.returncode, reds)]


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-04G mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
    survivors = invalid = 0
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
            invalid += 1
            continue
        try:
            open(path, 'wb').write(text.replace(o, n).encode('utf-8'))
            code, out = build()
            if code != 0:
                lines.append('%s [%s]: BUILD FAILED (INVALID, not killed)' % (label, rel))
                lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                invalid += 1
                continue
            results = test()
        finally:
            open(path, 'wb').write(original)
            os.utime(path, None)
        killed = any(code != 0 for _, code, _ in results)
        survivors += 0 if killed else 1
        lines.append('%s [%s]: %s' % (label, rel, 'KILLED' if killed else 'SURVIVED'))
        for t, code, reds in results:
            lines.append('    %s exit=%d  %d RED' % (t, code, len(reds)))
            lines += ['        ' + r[:200] for r in reds]
    bcode, _ = build()
    restored = test()
    lines.append('')
    lines.append('restored: build exit=%d; %s' % (bcode, ', '.join('%s exit=%d' % (t, c) for t, c, _ in restored)))
    lines.append('mutations: %d, killed: %d, survived: %d, invalid: %d' %
                 (len(selected), len(selected) - survivors - invalid, survivors, invalid))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    sys.exit(1 if survivors or invalid or bcode or any(c for _, c, _ in restored) else 0)


if __name__ == '__main__':
    main()
