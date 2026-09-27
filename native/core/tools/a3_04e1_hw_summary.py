#!/usr/bin/env python
"""A3-04E.1 hardware closeout (ALPHA3_AUDIO.md section 23.10): summarise a
serial capture of the A3-04E.1 image.

Reads one capture (UTF-8 or the UTF-16 a PowerShell tee writes) and prints:
  - the watchdog / crash / reboot search;
  - every A3E_PROBE line;
  - the A3E_PACE heartbeat sequence (gaps = the console dropped lines);
  - one row per A3E_PACE heartbeat (the cumulative window's fields);
  - the per-frame `render N us` lines of gameplay frames (crc != 0; the
    Developer screen draws with crc 00000000), bucketed by pacing phase.

The A3E_PACE counters are cumulative over the window, so after a probe
change their max fields still hold the earlier phase: the per-frame render
lines, bucketed by the probe that was active when they were drawn, are the
per-phase evidence.

usage: python a3_04e1_hw_summary.py CAPTURE [CAPTURE ...]
"""
import re
import statistics
import sys

PACE = re.compile(
    r'\((\d+)\) AlphaRuntime: A3E_PACE hb=(\d+) pace=\[([^\]]*)\] win=([\d.]+)s'
    r' \| frame n=(\d+) avg=(\S+) max=(\S+) \| tft=(\S+) in=(\S+)'
    r' \| full=(\S+) vp=(\S+) \| yld n=(\d+) /vp=(\S+) vpms=(\S+) tot=(\S+) max=(\S+) late=\d+'
    r' \| loop n=\d+ /s=(\d+) max=(\S+) wait=\S+ in=(\d+) asleep=(\S+)'
    r' \| idle0 gap=(\S+) forced=(\S+) miss=(\d+)'
    r' \| und=(\d+) hw=(\d+) miss=(\d+) \| cpu0=(\d+) cpu1=(\d+) main=(\d+) aud=(\d+)')
SCEN = re.compile(r'A3C_PERF scen=\[([^\]]*)\]')
HB = re.compile(r'AlphaRuntime: A3E_PACE hb=(\d+) ')
PROBE = re.compile(r'\((\d+)\) AlphaRuntime: A3E_PROBE (.*)')
RENDER = re.compile(r'\((\d+)\) AlphaRuntime: render (\d+) us reason=(\S+) animated=\d+ crc=(\w+)')
BAD = re.compile(r'task_wdt|Guru Meditation|abort\(\)|panic|Backtrace|rst:0x|ESP-ROM', re.I)


def read_lines(path):
    raw = open(path, 'rb').read()
    text = raw.decode('utf-16') if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else raw.decode('utf-8', 'replace')
    return text.replace('\r', '').split('\n')


def summarise(path):
    lines = read_lines(path)
    print('=' * 78)
    print(path, '-', len(lines), 'lines')
    bad = [l for l in lines if BAD.search(l)]
    print('watchdog / crash / reboot lines:', len(bad))
    for l in bad[:20]:
        print('  ', l[:160])
    probes = []
    for l in lines:
        m = PROBE.search(l)
        if m:
            probes.append((int(m.group(1)), m.group(2).strip()))
            print('probe t=%.3f s  %s' % (int(m.group(1)) / 1000, m.group(2).strip()))
    hbs, scen = [], None
    rows = []
    for l in lines:
        m = SCEN.search(l)
        if m:
            scen = m.group(1)
        m = HB.search(l)
        if m:
            hbs.append(int(m.group(1)))
        m = PACE.search(l)
        if m:
            g = m.groups()
            rows.append('t=%6.1f hb=%3s [%s] win=%6ss fr=%s avg=%s max=%s tft=%s full=%s vp=%s '
                        'yld/vp=%s vpms=%s loop/s=%s asleep=%ss idle0 gap=%s forced=%s miss=%s '
                        'und/hw/miss=%s/%s/%s cpu0/1=%s/%s | %s'
                        % (int(g[0]) / 1000, g[1], g[2], g[3], g[4], g[5], g[6], g[7], g[9], g[10],
                           g[12], g[13], g[16], g[19], g[20], g[21], g[22], g[23], g[24], g[25],
                           g[26], g[27], scen))
    gaps = [(a, b) for a, b in zip(hbs, hbs[1:]) if b != a + 1]
    print('A3E_PACE heartbeats (rows below: those with every field): %d, hb=%s..%s, non-consecutive: %s'
          % (len(hbs), hbs[0] if hbs else '-', hbs[-1] if hbs else '-', gaps or 'none'))
    for r in rows:
        print('  ' + r)
    cuts = [t for t, _ in probes]
    names = ['boot state'] + [p for _, p in probes]
    buckets = {}
    for l in lines:
        m = RENDER.search(l)
        if not m or m.group(4) == '00000000':
            continue
        t = int(m.group(1))
        phase = sum(t >= c for c in cuts)
        buckets.setdefault((phase, m.group(3)), []).append(int(m.group(2)) / 1000)
    print('gameplay frames (render lines, crc != 0), ms, by pacing phase:')
    for (phase, reason) in sorted(buckets):
        v = sorted(buckets[(phase, reason)])
        p90 = v[min(len(v) - 1, int(len(v) * 0.9))]
        print('  %-28s %-14s n=%4d min %6.1f med %6.1f p90 %6.1f max %6.1f'
              % (names[phase], reason, len(v), v[0], statistics.median(v), p90, v[-1]))


if __name__ == '__main__':
    for p in sys.argv[1:]:
        summarise(p)
