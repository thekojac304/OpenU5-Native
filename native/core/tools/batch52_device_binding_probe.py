"""Batch 52 -- run the quest parity corpus with the T-Deck's own service shape.

quest_parity passes because tests/quest_driver.cpp binds every QuestWorldServices
hook from the shipped data. AlphaRuntime::initialize() (native/targets/tdeck/main/
alpha_runtime.cpp) does not bind four of them. This probe rebuilds the real driver
with exactly those differences, one variant per hook plus the combined device shape,
and counts the shipped cases whose observable result changes.

Variants (each a textual edit of an unmodified copy of tests/quest_driver.cpp):
  control      -- no edit (must report 0 divergences)
  end_record   -- services.end_record left null (ENDMSG.DAT, never bound on device)
  words        -- services.words left empty (Words of Power, never bound on device)
  moonstones   -- services.moonstones / moonstone_count left unset (never bound)
  karma        -- services.karma_record = the device's fixed string (alpha_runtime.cpp
                  bind_rest_services supplies it; quest_.karma_record is unbound)
  device       -- all four at once, plus endgame_script/end_narration left unset

The driver's early exits that only exist to catch an unbound hook (14: absorption
refused, 23: encounter teardown refused) are turned into logged continues so the
comparison can count every affected case instead of stopping at the first.

The comparison is a copy of tools/check-quests.ts whose per-case throw is replaced by
a per-op tally; it is written next to the original (its imports are relative) and
deleted afterwards. Nothing tracked is modified.

Usage (repo root):  python native/core/tools/batch52_device_binding_probe.py [build-dir]
"""
import json, os, re, subprocess, sys, tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CORE = os.path.join(ROOT, 'native', 'core')
BIN = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin'
GXX = BIN + '/g++.exe'
NODE = 'C:/Program Files/nodejs/node.exe'

END_RECORD = ('services.end_record=[](void *p,int32_t i)->const char *{auto &v=static_cast<WorldFixture *>(p)->records;'
              'return i>=0&&size_t(i)<v.size()?v[size_t(i)].c_str():nullptr;};')
WORDS = 'services.words={words.data(),words.size()};'
MOONS = 'services.moonstones=moons.data();services.moonstone_count=moons.size();'
KARMA = ('services.karma_record=[](void *p,int32_t i)->const char *{auto &v=static_cast<WorldFixture *>(p)->karma;'
         'return i>=0&&size_t(i)<v.size()?v[i].c_str():nullptr;};')
DEVICE_KARMA = 'services.karma_record=[](void*,int32_t)->const char *{return "\\"Rest well, Avatar. Continue upon the path of virtue.\\"";};'
SCRIPT = 'services.endgame_script=true;'
EXIT14 = 'if(absorption_endgame(ctx,ctx.events)==CommandStatus::InvalidContext)return 14;'
EXIT23 = 'if(ctx.combat&&finish_encounter_combat(ctx,*battle)!=CombatResult::Ok)return 23;'

def variant_source(src, name):
    def cut(s, old, new=''):
        if s.count(old) != 1:
            raise SystemExit(f'{name}: expected exactly one "{old[:40]}..." in quest_driver.cpp, found {s.count(old)}')
        return s.replace(old, new)
    s = src
    if name != 'control':
        # Never an assertion about the device: these only keep a refused case running.
        s = cut(s, EXIT14, 'if(absorption_endgame(ctx,ctx.events)==CommandStatus::InvalidContext)'
                           'std::cerr<<"BATCH52 absorption_endgame=InvalidContext"<<std::endl;')
        s = cut(s, EXIT23, 'if(ctx.combat&&finish_encounter_combat(ctx,*battle)!=CombatResult::Ok)'
                           'std::cerr<<"BATCH52 finish_encounter_combat!=Ok combat_still_active="<<ctx.combat<<std::endl;')
    if name in ('end_record', 'device'):
        s = cut(s, END_RECORD, '/* BATCH52: end_record unbound (device) */')
    if name in ('words', 'device'):
        s = cut(s, WORDS, '/* BATCH52: words unbound (device) */')
    if name in ('moonstones', 'device'):
        s = cut(s, MOONS, '/* BATCH52: moonstones unbound (device) */')
    if name in ('karma', 'device'):
        s = cut(s, KARMA, DEVICE_KARMA)
    if name == 'device':
        s = cut(s, SCRIPT, '/* BATCH52: endgame_script unset (device) */')
    return s

def counting_check(src):
    old = ("if(!isDeepStrictEqual(actual[i],expected[i])) {writeFileSync(dir+'/mismatch.json',"
           "JSON.stringify({input:inputs[i],expected:expected[i],actual:actual[i]},null,2));"
           "throw new Error(`Quest mismatch ${i}: ${inputs[i].op}`);}")
    if src.count(old) != 1:
        raise SystemExit('check-quests.ts comparison line not found; update the probe')
    # Per case group, the normalized result paths that differ (array indices elided),
    # so a text-only divergence is distinguishable from a state divergence.
    new = ("if(!isDeepStrictEqual(actual[i],expected[i])) {const op=inputs[i].op;"
           "const kinds=(inputs[i].actions??[]).map((a:any)=>a.kind);"
           "const key=op+(op==='world-flow'?' ['+[...new Set(kinds)].sort().join(',')+']':'');"
           "(globalThis as any).__b52=(globalThis as any).__b52??{total:0,byOp:{},byFlow:{},paths:{}};"
           "const t=(globalThis as any).__b52;t.total++;t.byOp[op]=(t.byOp[op]??0)+1;t.byFlow[key]=(t.byFlow[key]??0)+1;"
           "const found=new Set<string>();const walk=(e:any,a:any,p:string)=>{if(found.size>=8)return;"
           "if(isDeepStrictEqual(e,a))return;"
           "if(e&&a&&typeof e==='object'&&typeof a==='object'&&Array.isArray(e)===Array.isArray(a)){"
           "if(Array.isArray(e)&&e.length!==a.length){found.add(p+'[] length');return;}"
           "for(const k of new Set([...Object.keys(e),...Object.keys(a)]))walk(e[k],a[k],Array.isArray(e)?p+'[]':p+'.'+k);return;}"
           "found.add(p);};walk(expected[i],actual[i],'');"
           "t.paths[key]=[...new Set([...(t.paths[key]??[]),...found])].slice(0,8);}")
    s = src.replace(old, new)
    s += ("\nconsole.log('BATCH52_SUMMARY '+JSON.stringify({cases:expected.length,"
          "mismatches:(globalThis as any).__b52??{total:0,byOp:{},byFlow:{},paths:{}}}));\n")
    return s

def main():
    build = sys.argv[1] if len(sys.argv) > 1 else os.path.join(CORE, 'build-batch52-baseline')
    lib = os.path.join(build, 'libopenu5_core.a')
    driver = open(os.path.join(CORE, 'tests', 'quest_driver.cpp'), encoding='utf-8', newline='').read()
    check = open(os.path.join(CORE, 'tools', 'check-quests.ts'), encoding='utf-8', newline='').read()
    tmp_check = os.path.join(CORE, 'tools', '.batch52-counting-check-quests.ts')
    env = dict(os.environ, PATH=BIN.replace('/', os.sep) + os.pathsep + os.environ.get('PATH', ''))
    results = {}
    with tempfile.TemporaryDirectory() as work:
        open(tmp_check, 'w', encoding='utf-8', newline='').write(counting_check(check))
        try:
            for name in ('control', 'end_record', 'words', 'moonstones', 'karma', 'device'):
                cpp = os.path.join(work, f'quest_driver_{name}.cpp')
                exe = os.path.join(work, f'quest_driver_{name}.exe')
                open(cpp, 'w', encoding='utf-8', newline='').write(variant_source(driver, name))
                subprocess.run([GXX, '-std=gnu++20', '-O3', '-DNDEBUG', '-DOPENU5_ENABLE_DEVELOPER_TOOLS=1',
                                '-I' + os.path.join(CORE, 'include'), '-I' + os.path.join(CORE, 'tests'),
                                cpp, lib, '-o', exe], check=True, env=env)
                run = subprocess.run([NODE, '--import', 'tsx', tmp_check, exe], cwd=ROOT,
                                     capture_output=True, text=True, encoding='utf-8', env=env)
                line = next((l for l in run.stdout.splitlines() if l.startswith('BATCH52_SUMMARY ')), None)
                if run.returncode != 0 or line is None:
                    results[name] = {'error': (run.stderr or run.stdout)[-2000:], 'rc': run.returncode}
                else:
                    results[name] = json.loads(line[len('BATCH52_SUMMARY '):])
                print('==', name, json.dumps(results[name], sort_keys=True, indent=1), flush=True)
        finally:
            os.remove(tmp_check)
    if results.get('control', {}).get('mismatches', {}).get('total') != 0:
        raise SystemExit('control variant diverged; the probe is invalid')

if __name__ == '__main__':
    main()
