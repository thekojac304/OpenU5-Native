"""Commit A mutation proof: each mutant must turn at least one P check RED."""
import os, shutil, subprocess, sys, time
ROOT = r'.'
CORE = ROOT + r'\native\core'
NINJA = r'C:\Dev\TamaPoke\.build-tools\w64devkit\bin\ninja.exe'
F = ROOT + r'\native\targets\tdeck\main\alpha_resources.cpp'
MUTANTS = {
    'MA1 drop "ibm.ch" from required[]': ('"runes.ch","ibm.ch","combatmaps.json"', '"runes.ch","combatmaps.json"'),
    'MA2 load the runes into ibm_font': ('read(*ibm,0,o.ibm_font,1024)', 'read(*runes,0,o.ibm_font,1024)'),
    'MA3 never load ibm_font': ('o.ibm_font=static_cast<uint8_t*>(psram_alloc(1024));if(!o.ibm_font)', 'o.ibm_font=nullptr;if(false)'),
}
env = dict(os.environ, PATH=r'C:\Dev\TamaPoke\.build-tools\w64devkit\bin;' + os.environ['PATH'])
orig = open(F, 'rb').read()
results = []
try:
    for name, (a, b) in MUTANTS.items():
        s = orig.decode('utf8')
        assert s.count(a) == 1, name
        open(F, 'wb').write(s.replace(a, b).encode('utf8'))
        build = subprocess.run([NINJA, '-C', CORE + r'\build-a4-ui1', 'a4_ui1_chrome_runtime'], env=env, capture_output=True, text=True)
        if build.returncode:
            results.append((name, 'INVALID (build failed)'))
            continue
        run = subprocess.run([CORE + r'\build-a4-ui1\a4_ui1_chrome_runtime.exe', ROOT + r'\native\assets\openu5-alpha1-resources.bin',
                              ROOT + r'\original\u5\ultima5\IBM.CH'], capture_output=True, text=True)
        reds = [l.split()[1] for l in run.stdout.splitlines() if l.startswith('RED ')]
        results.append((name, 'KILLED by ' + ','.join(reds) if reds else 'SURVIVED'))
finally:
    open(F, 'wb').write(orig)
    os.utime(F, None)
    time.sleep(1)
    os.utime(F, None)
    subprocess.run([NINJA, '-C', CORE + r'\build-a4-ui1', 'a4_ui1_chrome_runtime'], env=env, capture_output=True)
for r in results:
    print(f'{r[0]}: {r[1]}')
