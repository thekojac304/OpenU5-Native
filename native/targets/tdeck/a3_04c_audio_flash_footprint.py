#!/usr/bin/env python
"""Alpha 3 A3-04C -- what the audio task still executes from FLASH per block.

ALPHA3_AUDIO.md section 20.6. A3-04B moved every per-sample function into
IRAM (a3_04b_iram_check.py proves it). What is left on the shared flash
instruction cache is the per-BLOCK loop around it: TdeckAudioBackend::run(),
AudioRingPump::step(), the sink's write and the ESP-IDF write path. This
walks the linked image's call graph from those roots (call8 and l32r+callx),
a few levels deep, and lists every FLASH function on the normal path with
its size, so the residual cross-core cache footprint is a number, not a guess.

Not followed: error/log/abort paths (esp_log_*, printf family, abort,
__assert_func, esp_err_to_name, i2s error helpers), whose code never runs on
the normal path, and anything already in IRAM/ROM (not on the flash cache).
Static only: it says what CAN run per block, an upper bound on the normal
path; the device's per-row audio busy/idle split measures the effect.

Usage: python a3_04c_audio_flash_footprint.py <openu5_tdeck.elf> [<log>] [--depth N]
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import a3_04b_iram_check as iram  # noqa: E402

ROOTS = [
    'tdeck::TdeckAudioBackend::run()',
    'openu5::AudioRingPump::step(openu5::PcmRingSink&, unsigned short, unsigned short)',
    'tdeck::TdeckAudioBackend::I2sRingSink::write(short const*)',
]
SKIP = re.compile(r'esp_log|printf|vfprintf|puts|abort|__assert|esp_err_to_name|_panic|esp_system_abort|'
                  r'__cxa_|std::__throw|__ubsan|ESP_ERROR_CHECK|esp_backtrace')
BYTES_PER_INSN = 2.6  # Xtensa: a mix of 2- and 3-byte encodings (measured on this image below when possible)


def main():
    elf = sys.argv[1]
    log = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith('--') else None
    depth = 4
    if '--depth' in sys.argv:
        depth = int(sys.argv[sys.argv.index('--depth') + 1])
    funcs, calls, sizes, _ = iram.disassemble(elf)
    lines = ['# A3-04C audio per-block flash footprint: %s' % os.path.abspath(elf), '']
    missing = [r for r in ROOTS if r not in funcs]
    if missing:
        lines.append('roots not found: %s' % ', '.join(missing))
    seen = {}
    frontier = [(r, 0) for r in ROOTS if r in funcs]
    for r, _ in frontier:
        seen[r] = 0
    while frontier:
        nxt = []
        for f, d in frontier:
            if d >= depth:
                continue
            for c in calls.get(f, []):
                if c in seen or SKIP.search(c):
                    continue
                # The per-sample path is A3-04B's (IRAM, checked separately): not followed.
                if c == iram.ROOT:
                    continue
                seen[c] = d + 1
                nxt.append((c, d + 1))
        frontier = nxt
    by_where = {}
    for f in seen:
        by_where.setdefault(iram.where(funcs.get(f, 0)), []).append(f)
    flash = sorted(by_where.get('FLASH', []), key=lambda n: -sizes.get(n, 0))
    total = sum(sizes.get(f, 0) for f in flash)
    lines.append('reached (depth <= %d, error/log paths not followed): %d functions -- %s' % (
        depth, len(seen), ', '.join('%s %d' % (k, len(v)) for k, v in sorted(by_where.items()))))
    lines.append('')
    lines.append('FLASH functions (instructions, depth from a root):')
    for f in flash:
        lines.append('    %5d  d%d  %s' % (sizes.get(f, 0), seen[f], f[:110]))
    approx = int(total * BYTES_PER_INSN)
    lines.append('')
    lines.append('total %d instructions in flash (~%d bytes, ~%d 32-byte ICache lines of the 16 KB / 512-line cache) '
                 '-- an upper bound on what one block can fetch through the shared cache' % (
                     total, approx, (approx + 31) // 32))
    text = '\n'.join(lines) + '\n'
    print(text)
    if log:
        open(log, 'w', newline='\n').write(text)


if __name__ == '__main__':
    main()
