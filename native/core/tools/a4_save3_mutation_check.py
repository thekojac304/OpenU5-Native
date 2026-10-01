"""Alpha 4 A4-SAVE3 -- mutation check of the PC save import/export bridge.

    python tools/a4_save3_mutation_check.py <build-dir> [ids,comma,separated]

Each mutant is one or more edits of production code (anchors converted to the
file's own line endings). The driver applies them, touches the files, builds
the named test target(s), runs them, and restores + touches the files whatever
happens (a restored file older than the mutated object would leave the mutant
in the build). A mutant is KILLED when a named test exits non-zero, INVALID
when it does not build. The PATH must hold the host toolchain (w64devkit).

The classes the A4-SAVE3 brief names: an incomplete set accepted (P1), wrong
JSON/default reconstruction (P2-P4), the wrong destination slot (P5), the
source modified (P6), the destination destroyed before the new generation
validates (P7), Native-only metadata in the export (P8, P9), the exporter
choosing the wrong or a failing generation (P10, P11), an incompatible source
accepted (P12, P13); then the bridge (vehicles, bitmaps, time spell), the
overwrite / import-again questions, the fresh map entry and the outdoor
restore, the slot left untouched by export, and the dungeon boundary.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
NINJA = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/ninja.exe'
NODE = 'C:/Program Files/nodejs/node.exe'
PACK = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
FIXTURE = os.path.join(ROOT, 'original/u5/ultima5')
PC = 'native/core/src/pc_save.cpp'
SAVE = 'native/targets/tdeck/main/alpha_save.cpp'
RUNTIME = 'native/targets/tdeck/main/alpha_runtime.cpp'
FRONTEND = 'native/core/src/frontend.cpp'
# key: (target to build, argv builder)
TESTS = {
    'pc': ('a4_save3_pc_bridge_runtime', lambda b: [os.path.join(b, 'a4_save3_pc_bridge_runtime.exe'), PACK, FIXTURE]),
    'ref': ('a4_save3_pc_bridge_runtime', lambda b: [NODE, '--import', 'tsx', os.path.join(ROOT, 'native/core/tools/check-pc-save.ts'),
                                                     os.path.join(b, 'a4_save3_pc_bridge_runtime.exe'), PACK, FIXTURE]),
    'slots': ('a4_save2_slots_runtime', lambda b: [os.path.join(b, 'a4_save2_slots_runtime.exe'), PACK]),
    'frontend': ('frontend_tests', lambda b: [os.path.join(b, 'frontend_tests.exe'), os.path.join(ROOT, 'game/assets/init.gam'),
                                              os.path.join(ROOT, 'game/assets/init.ool'), os.path.join(ROOT, 'original/u5/ultima5/MISCMAPS.DAT')]),
}

# (id, what, [(file, anchor, replacement), ...], tests)
MUTANTS = [
    ('P1', 'an incomplete set accepted: a missing SAVED.OOL becomes an empty block', [
        (SAVE, '    if(ool_size<0)return PcRead::NoOol;\n', ''),
        (SAVE, '    if(src.ool_size==openu5::save::kOolSize&&!read_file(ool,src.ool))return PcRead::ReadFailed;',
               '    if(ool_size<0){src.ool.assign(openu5::save::kOolSize,0);src.ool_size=openu5::save::kOolSize;}else if(src.ool_size==openu5::save::kOolSize&&!read_file(ool,src.ool))return PcRead::ReadFailed;'),
     ], ['pc']),
    ('P2', 'wrong default: wind not read from 0x2EC (B2)', [(PC, 's["wind"] = J(int(gam[kWind]));', 's["wind"] = J(0);')], ['pc', 'ref']),
    ('P3', 'wrong default: transport left at the empty sidecar\'s "foot" (B9)', [
        (PC, '    s["transport"] = J(kModes[unsigned(transport_mode(gam[kTransportTile]))]);\n', '')], ['pc']),
    ('P4', 'wrong reconstruction: the found-once bitmap read one bit off (B1)', [
        (PC, 'if (search_gated(i) || !(gam[kSearchFound + i / 8] & (1u << (i % 8)))) continue;',
             'if (search_gated(i) || !(gam[kSearchFound + i / 8] & (1u << ((i + 1) % 8)))) continue;')], ['pc']),
    ('P5', 'wrong destination: the import saves to the slot saved last, not the one chosen', [
        (RUNTIME, 'resources_.initial_ool,resources_.initial_ool_size,ms,false,slot)){', 'resources_.initial_ool,resources_.initial_ool_size,ms,false,-1)){')],
     ['pc']),
    ('P6', 'the source modified: the import marker is written over import/SAVED.GAM', [
        (SAVE, 'return make_directory("/sd/ultima5")&&replace_file(kImportMarker,text,size_t(n));',
               'return make_directory("/sd/ultima5")&&replace_file(std::string(kImportDirectory)+"/SAVED.GAM",text,size_t(n));')], ['pc']),
    ('P7', 'the destination destroyed first: the save writes over the generation it keeps', [
        (SAVE, 'const int slot=logical,gen=target.slot;', 'const int slot=logical,gen=target.keep>=0?target.keep:target.slot;')], ['pc']),
    ('P8', 'Native-only metadata exported: the sidecar goes into the export folder', [
        (SAVE, 'if(ok)ok=replace_file(in_dir(dir,"EXPORT.TXT"),manifest.data(),manifest.size());',
               'if(ok)ok=replace_file(in_dir(dir,"EXPORT.TXT"),manifest.data(),manifest.size())&&replace_file(in_dir(dir,"SAVED.JSN"),"{\\"version\\":1}",13);')],
     ['pc']),
    ('P9', 'Native-only metadata exported: SAVED.GAM carries a U5PARTIDA envelope', [
        (SAVE, 'if(ok)ok=replace_file(gam_path,gam.data(),gam.size());',
               'if(ok){std::string env(reinterpret_cast<const char*>(gam.data()),gam.size());env+="U5PARTIDA1\\n{}";ok=replace_file(gam_path,env.data(),env.size());}')],
     ['pc']),
    ('P10', 'the exporter prefers the older generation', [(SAVE, 'const int g=pass?1-newest:newest;', 'const int g=pass?newest:1-newest;')], ['pc']),
    ('P11', 'the exporter exports a generation the gate refused', [
        (SAVE, 'if(read_files(slot,g,v)&&verify_candidate(v,s.stage))chosen=g;', 'if(read_files(slot,g,v)){verify_candidate(v,s.stage);chosen=g;}')], ['pc']),
    ('P12', 'an incompatible source accepted: a dungeon save passes the check', [(PC, '    if (location >= 33) return Check::Dungeon;\n', '')], ['pc']),
    ('P13', 'an incompatible source accepted: any party size passes', [(PC, '    if (party < 1 || party > 6) return Check::Party;\n', '')], ['pc']),
    ('P14', 'a wrong-size SAVED.GAM accepted (longer than 4192)', [(PC, 'if (!gam || gam_size != kGamSize) return Check::GamSize;', 'if (!gam || gam_size < kGamSize) return Check::GamSize;')], ['pc']),
    ('P15', 'B10 import off: vehicle records are not converted', [
        (PC, '    for (int i = 1; i < 32; ++i) {\n        const uint8_t *o = table + i * 8;\n        const int b = o[0];',
             '    for (int i = 1; i < 1; ++i) {\n        const uint8_t *o = table + i * 8;\n        const int b = o[0];')],
     ['pc', 'ref']),
    ('P16', 'B10 export off: pool ships are not written', [
        (PC, '        put(floor, int(o["x"].integer()), int(o["y"].integer()), tile, int(o["hull"].integer()), int(o["skiffs"].integer()));',
             '        (void)tile;')], ['pc', 'ref']),
    ('P17', 'B10 export keeps the template\'s own vehicle records (the new-game skiff twice)', [
        (PC, '            if (frigate(table[i * 8]) || skiff(table[i * 8]) || horse(table[i * 8])) std::fill(table + i * 8, table + i * 8 + 8, uint8_t(0));',
             '            (void)table;')], ['pc', 'ref']),
    ('P18', 'B1 export writes the gated entries 13/14/15', [
        (PC, '''    for (int i = 0; i < kSearchEntries; ++i) {
        if (search_gated(i)) continue;
        char key[16];
        std::snprintf(key, sizeof(key), "search:%d", i);
        const uint8_t bit''', '''    for (int i = 0; i < kSearchEntries; ++i) {
        char key[16];
        std::snprintf(key, sizeof(key), "search:%d", i);
        const uint8_t bit''')], ['pc']),
    ('P19', 'B8 export drops the time spell', [(PC, 'b[kTimeSpell] = uint8_t(spell.string[0]);', 'b[kTimeSpell] = 0;')], ['pc']),
    ('P20', 'the overwrite question starts on Yes', [(FRONTEND, 'else{pc_confirm_=true;cursor_=0;}}', 'else{pc_confirm_=true;cursor_=1;}}')], ['pc']),
    ('P21', 'an occupied slot is imported over without asking', [
        (FRONTEND, 'if(catalog_.slots[cursor_].status==SaveSlotStatus::Empty&&pc_status_.imported_slot<0){', 'if(pc_status_.imported_slot<0){')], ['pc']),
    ('P22', 'files imported before are imported again without asking', [
        (FRONTEND, 'if(catalog_.slots[cursor_].status==SaveSlotStatus::Empty&&pc_status_.imported_slot<0){', 'if(catalog_.slots[cursor_].status==SaveSlotStatus::Empty){')],
     ['pc']),
    ('P23', 'the marker matches any files', [(SAVE, 'return g==gam_crc&&o==ool_crc&&slot>=0&&slot<kSlots?slot:-1;', 'return slot>=0&&slot<kSlots?slot:-1;')], ['pc']),
    ('P24', 'no fresh map entry: an imported town has nobody in it', [(RUNTIME, '    if(loc>=1&&loc<=32){\n        openu5::hydrate_interior_objects(context_,loc);', '    if(false){\n        openu5::hydrate_interior_objects(context_,loc);')],
     ['pc']),
    ('P25', 'no outdoor restore before the sync: horses, skiffs and monsters lost (RED-first of the T5 fix)', [
        (RUNTIME, '''    if(openu5::save::restore_gameplay(retained_,commands_,outdoor_)!=openu5::save::Error::None||
       openu5::save::restore_terrain(retained_,terrain_)!=openu5::save::Error::None)
        return say''', '''    if(false)
        return say''')], ['pc', 'ref']),
    ('P26', 'the export changes the slot it exports', [
        (SAVE, '    out.generation=chosen;out.sequence=v.commit.sequence;\n',
               '    out.generation=chosen;out.sequence=v.commit.sequence;write_file(path(slot,chosen,"gam"),gam.data(),gam.size());\n')], ['pc']),
    ('P27', 'a dungeon document exported', [(PC, 'if ((location >= 33 && location <= 40) || s["dungeon"].kind == J::Object) return Check::Dungeon;',
                                              '(void)location;')], ['pc']),
    ('P28', 'the export goes to the wrong slot folder', [(SAVE, 'std::snprintf(dir,sizeof(dir),"%s/slot%d",kExportDirectory,slot+1);',
                                                          'std::snprintf(dir,sizeof(dir),"%s/slot%d",kExportDirectory,slot+2);')], ['pc']),
    ('P29', 'the page is not told the import went through (stale "imported before")', [
        (RUNTIME, '''        openu5::FrontendSaveCatalog catalog{};save_.inspect_catalog(catalog);frontend_.set_save_catalog(catalog);
        frontend_.set_pc_import_status(inspect_pc_import());
        ESP_LOGI''', '''        openu5::FrontendSaveCatalog catalog{};save_.inspect_catalog(catalog);frontend_.set_save_catalog(catalog);
        ESP_LOGI''')], ['pc']),
    ('P30', 'a failed read is imported anyway (a short read is not refused)', [
        (SAVE, '    if((!src.gam.empty()&&src.gam.size()!=src.gam_size)||(!src.ool.empty()&&src.ool.size()!=src.ool_size))return PcRead::ReadFailed;\n', ''),
        (SAVE, '    if(src.gam_size==openu5::save::kGamSize&&!read_file(gam,src.gam))return PcRead::ReadFailed;',
               '    if(src.gam_size==openu5::save::kGamSize&&!read_file(gam,src.gam)){src.gam.assign(openu5::save::kGamSize,0);}'),
     ], ['pc']),
]


def touch(p):
    t = time.time()
    os.utime(p, (t, t))


def build(build_dir, targets):
    r = subprocess.run([NINJA, '-C', build_dir] + sorted(set(targets)), capture_output=True, text=True)
    return r.returncode == 0, r.stdout[-1500:]


def run(build_dir, key):
    _, argv = TESTS[key]
    r = subprocess.run(argv(build_dir), capture_output=True, text=True, cwd=ROOT)
    reds = [l.split()[1] for l in (r.stdout + r.stderr).splitlines() if l.strip().startswith('RED') and len(l.split()) > 1]
    return r.returncode, reds


def main():
    build_dir = os.path.abspath(sys.argv[1])
    only = set(sys.argv[2].split(',')) if len(sys.argv) > 2 else None
    killed = invalid = survived = 0
    for mid, what, edits, keys in MUTANTS:
        if only and mid not in only:
            continue
        originals = {}
        try:
            texts = {}
            bad = None
            for rel, _, _ in edits:
                path = os.path.join(ROOT, rel)
                if path not in originals:
                    originals[path] = open(path, 'rb').read()
                    texts[path] = originals[path].decode('utf-8')
            for rel, anchor, repl in edits:
                path = os.path.join(ROOT, rel)
                eol = '\r\n' if '\r\n' in texts[path] else '\n'
                a = anchor.replace('\r\n', '\n').replace('\n', eol)
                b = repl.replace('\r\n', '\n').replace('\n', eol)
                if texts[path].count(a) != 1:
                    bad = f'{texts[path].count(a)} matches in {rel}'
                    break
                texts[path] = texts[path].replace(a, b)
            if bad:
                print(f'{mid} ANCHOR-MISSING ({bad}) -- {what}', flush=True)
                invalid += 1
                continue
            for path, text in texts.items():
                open(path, 'wb').write(text.encode('utf-8'))
                touch(path)
            ok, out = build(build_dir, [TESTS[k][0] for k in keys])
            if not ok:
                print(f'{mid} INVALID (does not build) -- {what}\n{out}', flush=True)
                invalid += 1
                continue
            results = [(k,) + run(build_dir, k) for k in keys]
            dead = any(code != 0 for _, code, _ in results)
            detail = '; '.join(f'{k} exit={code} RED={",".join(r) or "-"}' for k, code, r in results)
            print(f'{mid} {"KILLED" if dead else "SURVIVED"} -- {what} -- {detail}', flush=True)
            killed += dead
            survived += not dead
        finally:
            for path, data in originals.items():
                open(path, 'wb').write(data)
                touch(path)
    ok, _ = build(build_dir, [t for t, _ in TESTS.values()])
    green = ok and all(run(build_dir, k)[0] == 0 for k in TESTS)
    print(f'A4-SAVE3 mutation check: {killed} killed, {survived} survived, {invalid} invalid; '
          f'restored build {"GREEN" if green else "RED"}')


if __name__ == '__main__':
    main()
