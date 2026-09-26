#!/usr/bin/env python
"""Alpha 3 A3-04A -- what the FIRMWARE's music synth actually calls per sample.

The host suite cannot see A3-04's stutter: on x86 a function-local static's
"already constructed?" test is an inline load, while the ESP32-S3 firmware,
built with -mdisable-hardware-atomics, calls __cxa_guard_acquire (a FreeRTOS
mutex take + give) on every access. This walks the LINKED image's call graph
from openu5::OplEmulator::generate (the per-chip-sample loop) and reports
which functions it can reach.

GREEN: neither __cxa_guard_acquire nor any FreeRTOS queue/semaphore/critical-
section entry point is reachable from the per-sample path.

Usage: python a3_04a_hotpath_check.py <openu5_tdeck.elf> [<log>]
"""
import os
import re
import subprocess
import sys

OBJDUMP = r'C:/Espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump.exe'
ROOT = 'openu5::OplEmulator::generate(float*, float*, unsigned int, unsigned int)'
FORBIDDEN = ['__cxa_guard_acquire', '__cxa_guard_release', 'xQueueSemaphoreTake', 'xQueueGenericSend',
             'xQueueReceive', 'vPortEnterCritical', 'xPortEnterCriticalTimeout', 'malloc', 'heap_caps_malloc',
             'operator new(unsigned int)', 'esp_log_write', 'vTaskDelay']


def disassemble(elf):
    out = subprocess.run([OBJDUMP, '-d', '-C', '--no-show-raw-insn', elf], capture_output=True, text=True,
                         encoding='utf-8', errors='replace').stdout
    funcs, calls, sizes = {}, {}, {}
    current = None
    head = re.compile(r'^([0-9a-f]+) <(.+)>:$')
    # Only a call that lands on a function's ENTRY is a call: Xtensa keeps
    # literal pools and constant tables inside .text, and objdump decodes
    # them as instructions whose "targets" fall mid-function (<name+0x50>).
    call = re.compile(r'\scall(?:0|4|8|12)\s+[0-9a-f]+ <(.+)>\s*$')
    for line in out.splitlines():
        m = head.match(line)
        if m:
            current = m.group(2)
            funcs[current] = int(m.group(1), 16)
            calls.setdefault(current, [])
            sizes[current] = 0
            continue
        if current is None:
            continue
        if re.match(r'^\s*[0-9a-f]+:\s', line):
            sizes[current] += 1
            c = call.search(line)
            if c and not re.search(r'\+0x[0-9a-f]+$', c.group(1)):
                calls[current].append(c.group(1))
    return funcs, calls, sizes


def main():
    elf = sys.argv[1]
    log = sys.argv[2] if len(sys.argv) > 2 else None
    funcs, calls, sizes = disassemble(elf)
    lines = ['# A3-04A firmware hot-path check: %s' % os.path.abspath(elf), '']
    if ROOT not in funcs or sizes[ROOT] < 20:
        # Never pass on a parse failure: a checker that sees nothing is not a GREEN.
        lines.append('RED %s not found (or not parsed) in the image' % ROOT)
        print('\n'.join(lines))
        sys.exit(2)
    # Breadth-first over direct calls, recording one path to each function.
    parent = {ROOT: None}
    frontier = [ROOT]
    while frontier:
        nxt = []
        for f in frontier:
            for callee in calls.get(f, []):
                if callee not in parent:
                    parent[callee] = f
                    nxt.append(callee)
        frontier = nxt
    direct = calls[ROOT]
    lines.append('generate(): %d instructions, %d call sites: %s' % (
        sizes[ROOT], len(direct), ', '.join(sorted(set(direct))) or 'none'))
    lines.append('functions reachable from generate(): %d' % (len(parent) - 1))
    for f in sorted(parent):
        if f != ROOT:
            lines.append('    %-60s %5d instructions' % (f[:60], sizes.get(f, 0)))
    bad = [f for f in FORBIDDEN if f in parent]
    for f in bad:
        chain, g = [], f
        while g is not None:
            chain.append(re.sub(r'\([^()]*\)( const)?$', '', g))
            g = parent[g]
        lines.append('reachable: %s  via %s' % (f, ' <- '.join(chain)))
    lines.append('')
    lines.append(('RED the per-sample path reaches: ' + ', '.join(bad)) if bad else
                 'GREEN no guard, lock, allocation, log or delay is reachable from the per-sample path')
    text = '\n'.join(lines) + '\n'
    print(text)
    if log:
        open(log, 'w', newline='\n').write(text)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
