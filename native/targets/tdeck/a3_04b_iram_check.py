#!/usr/bin/env python
"""Alpha 3 A3-04B -- does the audio task's per-sample path still use the flash cache?

ALPHA3_AUDIO.md section 19.8. The ESP32-S3's instruction and data caches are
shared by both cores: code executed from flash (0x42000000..) and constants
read from flash .rodata (0x3C000000..) go through them and through the MSPI
bus the renderer on the other core also uses for flash and PSRAM. This walks
the LINKED image's call graph from the audio task's per-block render
(AudioRingPump::render_block) -- direct call8 targets AND the l32r+callx
long calls an IRAM function needs to reach flash -- and classifies every
function it reaches by where it lives.

GREEN:
  - every function reached is in IRAM (0x4037xxxx..0x403Dxxxx) or ROM
    (0x4000xxxx..0x4005xxxx), except the per-event / per-cue / one-time
    functions listed in ALLOWED_FLASH (the tree is not followed below them);
  - no reached IRAM function loads the address of flash .rodata (l32r
    literal in 0x3C000000..0x3DFFFFFF);
  - no guard, lock, allocation, log or delay is reachable (A3-04A's rule),
    except the one-time chip-buffer sizing, ALLOWED_FLASH too.

Usage: python a3_04b_iram_check.py <openu5_tdeck.elf> [<log>]
"""
import os
import re
import subprocess
import sys

OBJDUMPS = [
    r'C:/Espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump.exe',
    os.path.expanduser(r'~/.espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump.exe'),
]
ROOT = 'openu5::AudioRingPump::render_block(unsigned short, unsigned short)'
PER_SAMPLE = ['openu5::OplEmulator::generate(float*, float*, unsigned int, unsigned int)',
              'openu5::MusicSongPlayer::render(short*, unsigned int, unsigned long, unsigned short)',
              'openu5::SfxPlayer::render(short*, unsigned int, unsigned short)']
# Reached from the per-block path, but NOT per sample: a MIDI event (the voice
# allocator behind dispatch), a cue start (compile + queue), the chip buffer's
# one-time sizing, and the per-block copies. Flash is fine for these; their
# own callees are not followed.
ALLOWED_FLASH = {
    'openu5::MusicSongPlayer::dispatch(openu5::MusicEvent const&)': 'per MIDI event (the voice allocator)',
    'openu5::SfxPlayer::start_next()': 'per cue start (compile_sfx, queue)',
    'openu5::MusicSongPlayer::ensure_capacity(unsigned int)': 'once per song size: the chip buffer',
    '_ZN6openu515MusicSongPlayer15ensure_capacityEj$part$0': 'once per song size: the chip buffer',
    'memmove': 'per block (std::copy of the chip buffer)',
    'memcpy': 'per block',
    'memset': 'per block (std::fill of a silent block)',
}
FORBIDDEN = ['__cxa_guard_acquire', '__cxa_guard_release', 'xQueueSemaphoreTake', 'xQueueGenericSend',
             'xQueueReceive', 'vPortEnterCritical', 'xPortEnterCriticalTimeout', 'malloc', 'heap_caps_malloc',
             'operator new(unsigned int)', 'esp_log_write', 'vTaskDelay', 'floor', 'ceil', 'lround', 'pow']

IRAM = (0x40370000, 0x403E0000)
ROM = (0x40000000, 0x40060000)
DROM = (0x3C000000, 0x3E000000)
FLASH_TEXT = (0x42000000, 0x44000000)


def where(addr):
    if IRAM[0] <= addr < IRAM[1]:
        return 'IRAM'
    if ROM[0] <= addr < ROM[1]:
        return 'ROM'
    if FLASH_TEXT[0] <= addr < FLASH_TEXT[1]:
        return 'FLASH'
    return 'other'


def disassemble(elf):
    objdump = next((p for p in OBJDUMPS if os.path.exists(p)), None)
    if not objdump:
        sys.exit('objdump not found')
    out = subprocess.run([objdump, '-d', '-C', '--no-show-raw-insn', elf], capture_output=True, text=True,
                         encoding='utf-8', errors='replace').stdout
    funcs, calls, sizes, rodata = {}, {}, {}, {}
    by_addr = {}
    current = None
    head = re.compile(r'^([0-9a-f]+) <(.+)>:$')
    call = re.compile(r'\scall(?:0|4|8|12)\s+[0-9a-f]+ <(.+)>\s*$')
    # l32r aN, <pool> (<value> <symbol>): the loaded literal and what it names.
    lit = re.compile(r'\sl32r\s+a\d+,\s*[0-9a-f]+ <[^>]*>\s*\(([0-9a-f]+) <([^>]+)>\)')
    pending = []
    for line in out.splitlines():
        m = head.match(line)
        if m:
            current = m.group(2)
            funcs[current] = int(m.group(1), 16)
            by_addr[int(m.group(1), 16)] = current
            calls.setdefault(current, [])
            rodata.setdefault(current, [])
            sizes[current] = 0
            continue
        if current is None or not re.match(r'^\s*[0-9a-f]+:\s', line):
            continue
        sizes[current] += 1
        # Only a call that lands on a function's ENTRY is a call: literal pools
        # decode as fake instructions whose "targets" fall mid-function.
        c = call.search(line)
        if c and not re.search(r'\+0x[0-9a-f]+$', c.group(1)):
            calls[current].append(c.group(1))
        l = lit.search(line)
        if l:
            value = int(l.group(1), 16)
            pending.append((current, value, l.group(2)))
    for f, value, name in pending:
        # A literal holding a function's entry is a long call (l32r + callx).
        if value in by_addr and not re.search(r'\+0x[0-9a-f]+$', name):
            calls[f].append(by_addr[value])
        elif DROM[0] <= value < DROM[1]:
            rodata[f].append('%08x %s' % (value, name))
    return funcs, calls, sizes, rodata


def main():
    elf = sys.argv[1]
    log = sys.argv[2] if len(sys.argv) > 2 else None
    funcs, calls, sizes, rodata = disassemble(elf)
    lines = ['# A3-04B firmware IRAM check: %s' % os.path.abspath(elf), '']
    if ROOT not in funcs or sizes[ROOT] < 10:
        lines.append('RED %s not found (or not parsed) in the image' % ROOT)
        print('\n'.join(lines))
        sys.exit(2)
    parent = {ROOT: None}
    frontier = [ROOT]
    while frontier:
        nxt = []
        for f in frontier:
            if f in ALLOWED_FLASH:
                continue
            for callee in calls.get(f, []):
                if callee not in parent:
                    parent[callee] = f
                    nxt.append(callee)
        frontier = nxt

    def chain(f):
        out, g = [], f
        while g is not None:
            out.append(re.sub(r'\([^()]*\)( const)?$', '', g))
            g = parent[g]
        return ' <- '.join(out)

    flash, allowed, data = [], [], []
    lines.append('functions reached from render_block(): %d' % len(parent))
    for f in sorted(parent, key=lambda n: funcs.get(n, 0)):
        addr = funcs.get(f, 0)
        w = where(addr)
        note = ''
        if f in ALLOWED_FLASH:
            note = '  (allowed: %s)' % ALLOWED_FLASH[f]
            if w == 'FLASH':
                allowed.append(f)
        elif w == 'FLASH':
            flash.append(f)
            note = '  <-- FLASH on the per-sample path'
        lines.append('    %08x %-5s %4d insns  %s%s' % (addr, w, sizes.get(f, 0), f[:90], note))
        if w == 'IRAM':
            for r in rodata.get(f, []):
                data.append('%s reads flash .rodata %s' % (re.sub(r'\([^()]*\)( const)?$', '', f), r))
    lines.append('')
    for p in PER_SAMPLE:
        lines.append('per-sample %-60s %s' % (p[:60], where(funcs.get(p, 0)) if p in funcs else 'not found'))
    lines.append('')
    bad = [f for f in FORBIDDEN if f in parent and f not in ALLOWED_FLASH]
    for f in flash:
        lines.append('FLASH  %s' % chain(f))
    for d in data:
        lines.append('DROM   %s' % d)
    for f in bad:
        lines.append('BANNED %s  via %s' % (f, chain(f)))
    lines.append('')
    ok = not flash and not data and not bad and all(where(funcs.get(p, 0)) == 'IRAM' for p in PER_SAMPLE if p in funcs)
    if ok:
        lines.append('GREEN the per-sample audio path runs from IRAM/ROM and reads no flash .rodata; '
                     '%d per-event/one-time/per-block function(s) stay in flash by design' % len(allowed))
    else:
        lines.append('RED the per-sample audio path still uses the flash cache: %d flash function(s), %d flash '
                     '.rodata read(s), %d banned call(s)' % (len(flash), len(data), len(bad)))
    text = '\n'.join(lines) + '\n'
    print(text)
    if log:
        open(log, 'w', newline='\n').write(text)
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
