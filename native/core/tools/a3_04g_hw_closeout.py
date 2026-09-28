#!/usr/bin/env python
"""A3-04G hardware closeout (ALPHA3_AUDIO.md section 28.21): H-201 (combat
hit feedback, D-63) and H-202 (System Menu save inspection and the storage
heap) from one serial capture of the A3-04G image.

Reads one capture (UTF-8 or the UTF-16 a PowerShell tee writes) and prints:
  - the identity line and the watchdog / crash / storage-error search;
  - H-201: every COMBAT_HIT_CUE with its class (enemy row=-1 / party row),
    the combat-hit-cue frames that follow it, a death on the same tick, the
    combat exit, and the audio windows inside the fight;
  - H-202: every SAVE_INSPECT; each System Menu open from its Alt+M edge to
    the first full menu frame; every SD window (release-reserved-dma ->
    dma-reserve-restored); the heartbeat plateaus with internal + PSRAM
    free and their sum; every new heap_int_min with the storage windows in
    its sampling interval; saves and loads;
  - the audio / input health lines;
  - section 28.16's re-escalation triggers, evaluated.

Log timestamps are the ESP-IDF tick in ms (10 ms resolution here); the
key -> frame figure is the Alt+M edge's timestamp to the timestamp of the
SYSTEM_MENU_RENDER line, which is logged after that frame was sent.

usage: python a3_04g_hw_closeout.py CAPTURE
"""
import re
import statistics
import sys

T = r'^[IWE] \((\d+)\) '
IDENT = re.compile(r'IDENTITY firmware=(\S+) git=(\S+)')
BAD = re.compile(r'task_wdt|Guru Meditation|abort\(\)|panic|Backtrace|rst:0x|ESP-ROM|Rebooting', re.I)
STORAGE_BAD = re.compile(r'allocate_dma_buf|dma-reserve-restore-failed|LOW INTERNAL RAM|'
                         r'SAVE_SCRATCH allocation failed|sdmmc|diskio|status=[1-9]|result=(?!ok)\w+ esp_err')
ERRLVL = re.compile(r'^E \(')
CUE = re.compile(T + r'AlphaRuntime: COMBAT_HIT_CUE target=(\d+) cell=(\d+),(\d+) row=(-?\d+)')
CUE_RENDER = re.compile(T + r'AlphaRuntime: render (\d+) us reason=combat-hit-cue')
DEATH = re.compile(T + r'AlphaRuntime: ENEMY_ID phase=death actor=(\d+)')
COUNTS = re.compile(T + r'AlphaRuntime: COMBAT_COUNTS reason=(\S+) alive_hostile=(\d+) escaped=(\d+) dead=(\d+)')
COMBAT_END = re.compile(T + r'AlphaRuntime: (COMBAT_END|COMBAT_FINISH_BEGIN|combat finish|WORLD_TILE_RESTORE|UI_MODE from=combat to=explore)(.*)')
PRE_COMBAT = re.compile(T + r'AlphaRuntime: WORLD_PRE_COMBAT')
POST_COMBAT = re.compile(T + r'AlphaRuntime: WORLD_POST_COMBAT')
AUDIO = re.compile(T + r'AlphaRuntime: AUDIO_PERF song=(.+?) window_ms=(\d+) blocks=(\d+) missed=(\d+) '
                   r'underruns=(\d+) hw_underruns=(\d+) .*fill_min=(\S+) .*sfx=(\d+) .*runaway=(\d+) failures=(\d+)')
INSPECT = re.compile(T + r'AlphaSave: SAVE_INSPECT slot0=(\w+) slot1=(\w+) bytes=(\d+) commit_us=(\d+) '
                     r'read_us=(\d+) verify_us=(\d+) total_us=(\d+)')
MENU_KEY = re.compile(T + r'AlphaRuntime: INPUT_EDGE .* ui=(\S+) emitted=system-menu')
MENU_OPEN = re.compile(T + r'AlphaRuntime: SYSTEM_MENU action=toggle open=1')
MENU_RENDER = re.compile(T + r'AlphaRuntime: SYSTEM_MENU_RENDER reason=system-menu full_redraw=1 .* us=(\d+)')
SD_HEAP = re.compile(T + r'AlphaSave: SD_HEAP operation=(\S+) state=(\S+) free_internal=(\d+) free_dma=(\d+) '
                     r'largest_dma=(\d+) largest_internal=(\d+)')
HEARTBEAT = re.compile(T + r'AlphaRuntime: METRICS heartbeat internal=(\d+) psram=(\d+)')
SYSPERF = re.compile(T + r'AlphaRuntime: SYS_PERF .* heap_int=(\d+) heap_int_min=(\d+)')
SAVE = re.compile(T + r'AlphaSave: save generation=(\d+) slot=(\d+) time=(\d+) ms')
LOAD = re.compile(T + r'AlphaSave: load generation=(\d+) slot=(\d+) time=(\d+) ms status=(\d+)')
KBD = re.compile(T + r'(?:TDeckInput|M51|AlphaRuntime): .*(ESP_ERR_INVALID_RESPONSE|KEYBOARD_ERROR|KEYBOARD_RECOVER|INPUT_RESYNC)')
KBD_METRICS = re.compile(T + r'TDeckInput: KEYBOARD_METRICS reads=(\d+) errors=(\d+) .*recoveries=(\d+)')
QUEUE = re.compile(T + r'TDeckInput: INPUT_QUEUE depth=(\d+) high_water=(\d+) dropped=(\d+) queued=(\d+) consumed=(\d+)')


def read_lines(path):
    raw = open(path, 'rb').read()
    text = raw.decode('utf-16') if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else raw.decode('utf-8', 'replace')
    return text.replace('\r', '').split('\n')


def ms(t):
    return '%.2f s' % (t / 1000)


def section(title):
    print()
    print('=' * 78)
    print(title)
    print('=' * 78)


def storage_windows(lines):
    """Pairs each release-reserved-dma with the next dma-reserve-restored."""
    windows, cur = [], None
    for l in lines:
        m = SD_HEAP.search(l)
        if not m:
            continue
        t, op, state = int(m.group(1)), m.group(2), m.group(3)
        free, largest = int(m.group(4)), int(m.group(7))
        if op in ('release-reserved-dma', 'dma-reserve-created'):
            cur = dict(state=state, t0=t, free0=free, largest0=largest)
            if op == 'dma-reserve-created':
                cur.update(t1=t, free1=free, largest1=largest)
                windows.append(cur)
                cur = None
        elif op == 'dma-reserve-restored' and cur:
            cur.update(t1=t, free1=free, largest1=largest)
            windows.append(cur)
            cur = None
    return windows


def main(path):
    lines = read_lines(path)
    print(path, '-', len(lines), 'lines')
    for l in lines:
        m = IDENT.search(l)
        if m:
            print('identity: firmware=%s git=%s' % (m.group(1), m.group(2)))
    bad = [l for l in lines if BAD.search(l)]
    sbad = [l for l in lines if STORAGE_BAD.search(l)]
    elvl = [l for l in lines if ERRLVL.search(l)]
    print('watchdog / crash / reboot lines:', len(bad))
    for l in bad[:10]:
        print('  ', l[:160])
    print('storage error lines (section 28.16 trigger 1 + H-202 step 12):', len(sbad))
    for l in sbad[:10]:
        print('  ', l[:160])
    print('error-level (E) lines:', len(elvl))
    for l in elvl[:10]:
        print('  ', l[:160])

    # ---------------------------------------------------------------- H-201
    section('H-201 - combat hit feedback (D-63)')
    cues, renders, deaths, fights = [], [], {}, []
    fight = None
    for l in lines:
        m = PRE_COMBAT.search(l)
        if m:
            fight = [int(m.group(1)), None]
            fights.append(fight)
        m = POST_COMBAT.search(l)
        if m and fight:
            fight[1] = int(m.group(1))
        m = CUE.search(l)
        if m:
            cues.append(dict(t=int(m.group(1)), target=int(m.group(2)), cell=(int(m.group(3)), int(m.group(4))),
                             row=int(m.group(5))))
        m = CUE_RENDER.search(l)
        if m:
            renders.append((int(m.group(1)), int(m.group(2))))
        m = DEATH.search(l)
        if m:
            deaths[(int(m.group(1)), int(m.group(2)))] = True
    for f in fights:
        print('fight: %s -> %s (%.1f s)' % (ms(f[0]), ms(f[1]) if f[1] else 'open',
                                           ((f[1] or f[0]) - f[0]) / 1000))
    print('%-9s %-7s %-6s %-5s %-6s %-8s %s' % ('t', 'target', 'cell', 'row', 'class', 'frames', 'cue -> restore frame'))
    for i, c in enumerate(cues):
        end = cues[i + 1]['t'] if i + 1 < len(cues) else 1 << 62
        fr = [r for r in renders if c['t'] <= r[0] < end]
        kill = ' KILLING BLOW (death same tick)' if (c['t'], c['target']) in deaths else ''
        cls = 'enemy' if c['row'] < 0 else 'party'
        print('%-9s %-7d %d,%-4d %-5d %-6s %-8s %s%s' % (
            ms(c['t']), c['target'], c['cell'][0], c['cell'][1], c['row'], cls,
            '%d (%s)' % (len(fr), '/'.join('%.1f' % (u / 1000) for _, u in fr)),
            ('%d ms' % (fr[-1][0] - c['t'])) if fr else '-', kill))
    enemy = [c for c in cues if c['row'] < 0]
    party = [c for c in cues if c['row'] >= 0]
    print('cues: %d (enemy row=-1: %d, party: %d, rows %s); combat-hit-cue frames: %d' % (
        len(cues), len(enemy), len(party), sorted({c['row'] for c in party}), len(renders)))
    print('party rows = target - 1:', all(c['row'] == c['target'] - 1 for c in party))
    for l in lines:
        m = COUNTS.search(l)
        if m and m.group(2) == 'player-command' and m.group(3) == '0':
            print('  ', l[l.index('COMBAT_COUNTS'):][:120])
        m = COMBAT_END.search(l)
        if m and 'result=continue' not in m.group(3):
            print('  ', ms(int(m.group(1))), (m.group(2) + m.group(3))[:110])
    for f in fights:
        for l in lines:
            m = AUDIO.search(l)
            if m and f[0] <= int(m.group(1)) <= (f[1] or 1 << 62):
                print('   audio in fight %s: song=%s missed=%s underruns=%s hw_underruns=%s fill_min=%s sfx=%s' % (
                    ms(int(m.group(1))), m.group(2), m.group(5), m.group(6), m.group(7), m.group(8), m.group(9)))

    # ---------------------------------------------------------------- H-202
    section('H-202 - SAVE_INSPECT')
    inspects = []
    for l in lines:
        m = INSPECT.search(l)
        if m:
            v = [int(m.group(1))] + list(m.group(2, 3)) + [int(x) for x in m.group(4, 5, 6, 7, 8)]
            inspects.append(v)
            print('%-9s slot0=%-8s slot1=%-8s bytes=%-6d commit=%6.1f read=%6.1f verify=%6.1f total=%6.1f ms' % (
                ms(v[0]), v[1], v[2], v[3], v[4] / 1000, v[5] / 1000, v[6] / 1000, v[7] / 1000))
    cached = [v for v in inspects if v[1] == 'cached' and v[2] == 'cached']
    if cached:
        tot = [v[7] for v in cached]
        com = [v[4] for v in cached]
        print('cached: n=%d  total min/median/max = %.1f / %.1f / %.1f ms  commit %.1f-%.1f ms  '
              'read_us=0 all: %s  verify_us=0 all: %s  bytes=64 all: %s' % (
                  len(cached), min(tot) / 1000, statistics.median(tot) / 1000, max(tot) / 1000,
                  min(com) / 1000, max(com) / 1000, all(v[5] == 0 for v in cached),
                  all(v[6] == 0 for v in cached), all(v[3] == 64 for v in cached)))
        half = len(tot) // 2
        print('cached total, first half mean %.2f ms, second half mean %.2f ms (slowdown check)' % (
            statistics.mean(tot[:half]) / 1000, statistics.mean(tot[half:]) / 1000))

    section('H-202 - System Menu opens: Alt+M edge -> first full menu frame')
    opens, key = [], None
    pending = None
    for l in lines:
        m = MENU_KEY.search(l)
        if m:
            key = (int(m.group(1)), m.group(2))
            continue
        m = INSPECT.search(l)
        if m and key:
            pending = dict(key=key[0], ui=key[1], inspect=int(m.group(8)))
            continue
        m = MENU_OPEN.search(l)
        if m and pending:
            pending['open'] = int(m.group(1))
            continue
        m = MENU_RENDER.search(l)
        if m and pending and 'open' in pending:
            pending['render_t'] = int(m.group(1))
            pending['render_us'] = int(m.group(2))
            opens.append(pending)
            pending, key = None, None
    print('%-3s %-9s %-8s %-11s %-10s %-10s %s' % ('#', 'key', 'ui', 'inspect', 'menu frame', 'key->frame', 'inspect+frame'))
    for i, o in enumerate(opens, 1):
        print('%-3d %-9s %-8s %7.1f ms %7.1f ms %7d ms %9.1f ms' % (
            i, ms(o['key']), o['ui'], o['inspect'] / 1000, o['render_us'] / 1000,
            o['render_t'] - o['key'], (o['inspect'] + o['render_us']) / 1000))
    if opens:
        kf = [o['render_t'] - o['key'] for o in opens]
        fr = [o['render_us'] for o in opens]
        sm = [o['inspect'] + o['render_us'] for o in opens]
        print('opens: %d  key->frame min/median/max = %d / %d / %d ms  menu frame %.1f-%.1f ms  '
              'inspect+frame %.1f-%.1f ms' % (len(opens), min(kf), statistics.median(kf), max(kf),
                                               min(fr) / 1000, max(fr) / 1000, min(sm) / 1000, max(sm) / 1000))
        print('reference: A3-04E.1 first open, same method (a3-04e1-hw-soak.log): 299,268 -> 300,088 ms = 820 ms')

    section('H-202 - SD windows (release-reserved-dma -> dma-reserve-restored)')
    wins = storage_windows(lines)
    print('%-9s %-14s %9s %9s %9s %9s %9s' % ('t', 'state', 'free0', 'free1', 'delta', 'largest0', 'largest1'))
    groups = []
    for w in wins:
        k = (w['state'], w['free0'], w['free1'], w['largest0'], w['largest1'])
        if groups and groups[-1][0] == k:
            groups[-1][1] += 1
            continue
        groups.append([k, 1, w])
    for k, n, w in groups:
        print('%-9s %-14s %9d %9d %+9d %9d %9d%s' % (
            ms(w['t0']), k[0], k[1], k[2], k[2] - k[1], k[3], k[4], ('   x%d identical' % n) if n > 1 else ''))
    ins = [w for w in wins if w['state'] == 'save-inspect']
    print('save-inspect windows: %d; restored == released (0 B retained) in all: %s' % (
        len(ins), all(w['free1'] == w['free0'] for w in ins)))

    section('H-202 - heartbeat plateaus (internal + PSRAM free)')
    prev, first = None, None
    for l in lines:
        m = HEARTBEAT.search(l)
        if not m:
            continue
        t, i, p = int(m.group(1)), int(m.group(2)), int(m.group(3))
        if prev and (i, p) == prev[1:]:
            continue
        if first is None:
            first = i + p
        d = '' if prev is None else '  internal %+8d  psram %+8d  sum %+7d' % (i - prev[1], p - prev[2], i + p - prev[1] - prev[2])
        print('%-9s internal=%7d psram=%8d sum=%8d%s' % (ms(t), i, p, i + p, d))
        prev = (t, i, p)
    if prev:
        print('sum, last - first: %+d B' % (prev[1] + prev[2] - first))

    section('H-202 - heap_int_min (SYS_PERF, 5 s samples)')
    last, lastt = None, None
    for l in lines:
        m = SYSPERF.search(l)
        if not m:
            continue
        t, mn = int(m.group(1)), int(m.group(3))
        if mn != last:
            inside = [w for w in wins if (lastt or 0) <= w['t1'] and w['t0'] <= t]
            print('%-9s heap_int_min=%d (set in (%s, %s]; storage windows in that interval: %s)' % (
                ms(t), mn, ms(lastt) if lastt else 'boot', ms(t),
                ', '.join('%s %s-%s' % (w['state'], ms(w['t0']), ms(w['t1'])) for w in inside) or 'none'))
        last, lastt = mn, t

    section('H-202 - saves and loads')
    for l in lines:
        m = SAVE.search(l) or LOAD.search(l)
        if m:
            print('  ', l[l.index('AlphaSave:'):])

    section('Audio / input health')
    aud = [AUDIO.search(l) for l in lines]
    aud = [m for m in aud if m]
    clean = [m for m in aud if m.group(5) == m.group(6) == m.group(7) == '0' and m.group(10) == m.group(11) == '0']
    print('AUDIO_PERF lines: %d, all missed=0 underruns=0 hw_underruns=0 runaway=0 failures=0: %d' % (len(aud), len(clean)))
    if aud:
        m = aud[-1]
        print('last: %s song=%s window_ms=%s blocks=%s fill_min=%s sfx=%s' % (
            ms(int(m.group(1))), m.group(2), m.group(3), m.group(4), m.group(8), m.group(9)))
    for l in lines:
        m = KBD.search(l)
        if m:
            print('  ', l[:170])
    km = [KBD_METRICS.search(l) for l in lines]
    km = [m for m in km if m]
    if km:
        print('last KEYBOARD_METRICS: reads=%s errors=%s recoveries=%s' % km[-1].group(2, 3, 4))
    q = [QUEUE.search(l) for l in lines]
    q = [m for m in q if m]
    if q:
        print('last INPUT_QUEUE: high_water=%s dropped=%s queued=%s consumed=%s' % q[-1].group(3, 4, 5, 6))

    section('Section 28.16 re-escalation triggers')
    print('1 storage / DMA error lines: %d' % len(sbad))
    t2 = [w for w in wins if w['free1'] < w['free0'] - 4096]
    print('2 windows restored > 4 KiB below their release value: %d' % len(t2))
    for w in t2:
        print('    %s %s: %d -> %d (%+d)' % (ms(w['t0']), w['state'], w['free0'], w['free1'], w['free1'] - w['free0']))
    t3 = [w for w in wins if w['largest1'] < 32768]
    print('3 dma-reserve-restored with largest_internal < 32 KiB: %d (%s)' % (
        len(t3), ', '.join(sorted({'%s=%d' % (w['state'], w['largest1']) for w in t3}))))
    # Heartbeats grouped by the storage windows between them: only a run of
    # menu opens with no load or save in it can show a menu-open slope.
    edges = [w['t1'] for w in wins if w['state'] not in ('save-inspect',)]
    runs = {}
    for l in lines:
        m = HEARTBEAT.search(l)
        if not m:
            continue
        t = int(m.group(1))
        k = max([e for e in edges if e <= t] or [0])
        runs.setdefault(k, []).append(int(m.group(2)))
    print('4 heartbeat internal between loads / saves (opens in each span):')
    for k in sorted(runs):
        nxt = min([e for e in edges if e > k] or [1 << 62])
        n = len([o for o in opens if k <= o['key'] < nxt])
        v = runs[k]
        print('    from %-9s opens=%-3d heartbeats=%-3d internal %d..%d %s' % (
            ms(k), n, len(v), min(v), max(v), 'flat' if min(v) == max(v) else
            ('falling' if v == sorted(v, reverse=True) else 'not monotonic')))
    print('5 new heap_int_min outside a storage window: see the heap_int_min section')
    print('6 internal .data/.bss/IRAM growth: not applicable (no build in this closeout)')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
