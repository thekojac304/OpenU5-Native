#!/usr/bin/env python3
"""A3-04F (ALPHA3_AUDIO.md section 26): the render baseline in an existing
hardware capture, split by music state.

The A3-04E.1 soak (a3-04e1-hw-soak.log) never restarted its measurement
window, so every A3C_PERF / A3E_PACE / AUDIO_PERF figure is cumulative since
boot. A segment's own average is recovered from two heartbeats:
    avg_seg = (avg2 * n2 - avg1 * n1) / (n2 - n1)
(the logged averages carry 0.1 ms, so the recovered value is good to about
0.05 * n2 / (n2 - n1) ms, printed as +-). Maxima cannot be split and are not.

The per-frame `render N us` lines (gameplay frames that were not animation
ticks) are grouped by the music state of the heartbeat that follows them.

    python a3_04f_hw_baseline.py <capture.log>
"""
import re
import statistics
import sys

HB = re.compile(r'^I \((\d+)\) AlphaRuntime: A3C_PERF scen=\[music (\d+)%[^\]]*\] win=([\d.]+)s \| frame n=(\d+) avg=([\d.]+)'
                r' p95=[\d.]+ max=[\d.]+ late=\d+ \| cmp=([\d.]+)/[\d.]+ tiles=[\d.]+ tft=([\d.]+)/[\d.]+ in=\d+:[\d.]+'
                r' \| logic=[\d.]+/[\d.]+ vp=(\d+):([\d.]+)/[\d.]+ oth=(\d+):([\d.]+)/[\d.]+ \| split fill=([\d.]+) xfer=([\d.]+)'
                r'.*? rows=(\d+) .*?\| audio blk=(\d+) rnd=([\d.]+)/[\d.]+ .*? und=(\d+) hw=(\d+) miss=(\d+)'
                r'.*?\| cpu0=(\d+) cpu1=(\d+) main=(\d+) aud=(\d+)')
RENDER = re.compile(r'^I \((\d+)\) AlphaRuntime: render (\d+) us reason=([a-z-]+) animated=(\d+)')
PACE = re.compile(r'^I \((\d+)\) AlphaRuntime: A3E_PACE hb=(\d+) .*?\| loop n=(\d+) /s=\d+ max=[\d.]+ wait=\d+:[\d.]+/[\d.]+'
                  r' in=(\d+) asleep=([\d.]+) \| idle0 gap=([\d.]+) forced=(\d+):')
INPUT = re.compile(r'^W \((\d+)\) TDeckInput: INPUT_SERVICE render_block_us=(\d+)')


def main(path):
    beats, renders, paces, inputs = [], [], [], []
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            if m := HB.match(line):
                g = m.groups()
                beats.append(dict(t=int(g[0]), music=int(g[1]), n=int(g[3]), frame=float(g[4]), cmp=float(g[5]),
                                  tft=float(g[6]), vp_n=int(g[7]), vp=float(g[8]), oth_n=int(g[9]), oth=float(g[10]),
                                  fill=float(g[11]), xfer=float(g[12]), rows=int(g[13]), blk=int(g[14]),
                                  rnd=float(g[15]), und=int(g[16]), hw=int(g[17]), miss=int(g[18]),
                                  cpu0=int(g[19]), cpu1=int(g[20]), main=int(g[21]), aud=int(g[22])))
            elif m := RENDER.match(line):
                renders.append((int(m[1]), int(m[2]), m[3], int(m[4])))
            elif m := PACE.match(line):
                paces.append((int(m[1]), float(m[6]), int(m[7])))
            elif m := INPUT.match(line):
                inputs.append((int(m[1]), int(m[2])))
    if not beats:
        sys.exit('no A3C_PERF lines')
    # Segments: maximal runs of heartbeats with the same music percentage.
    segs, start = [], 0
    for i in range(1, len(beats) + 1):
        if i == len(beats) or beats[i]['music'] != beats[start]['music']:
            segs.append((start, i - 1))
            start = i
    print(f'capture: {path}')
    print(f'heartbeats {len(beats)}  uptime {beats[0]["t"] / 1000:.1f}-{beats[-1]["t"] / 1000:.1f} s')
    print()
    print('Per music segment (averages recovered from cumulative counters; boundary = the heartbeat before it):')
    for a, b in segs:
        lo = beats[a - 1] if a > 0 else None
        hi = beats[b]
        base = lo or dict(n=0, frame=0, cmp=0, tft=0, vp_n=0, vp=0, oth_n=0, oth=0, fill=0, xfer=0, rows=0, blk=0, rnd=0)

        def seg(key, nkey='n'):
            n = hi[nkey] - base[nkey]
            if n <= 0:
                return None, 0, 0
            v = (hi[key] * hi[nkey] - base[key] * base[nkey]) / n
            err = 0.05 * hi[nkey] / n
            return v, n, err

        frames = hi['n'] - base['n']
        print(f'  music {hi["music"]:>2}%  uptime {(lo["t"] if lo else 0) / 1000:7.1f}-{hi["t"] / 1000:7.1f} s  '
              f'frames {frames}  CPU0 {hi["cpu0"]} % CPU1 {hi["cpu1"]} % main {hi["main"]} % aud {hi["aud"]} % '
              f'(cumulative)  und={hi["und"]} hw={hi["hw"]} miss={hi["miss"]} (cumulative)')
        for key, label, nkey in (('frame', 'frame avg', 'n'), ('cmp', 'compose avg', 'n'), ('tft', 'TFT avg', 'n'),
                                 ('vp', 'viewport-frame TFT avg', 'vp_n'), ('oth', 'other-frame TFT avg', 'oth_n'),
                                 ('fill', 'TFT row building avg', 'n'), ('xfer', 'TFT SPI transfer avg', 'n')):
            v, n, err = seg(key, nkey)
            if v is None:
                print(f'      {label:24s} (no frames)')
            else:
                print(f'      {label:24s} {v:6.1f} ms +-{err:4.1f}  over {n} frames')
        rows = hi['rows'] - base['rows']
        if frames > 0:
            print(f'      {"transactions / frame":24s} {rows / frames:6.1f}  (rows {rows})')
        blk = hi['blk'] - base['blk']
        if blk > 0:
            v = (hi['rnd'] * hi['blk'] - base['rnd'] * base['blk']) / blk
            print(f'      {"audio render / 8 ms blk":24s} {v:6.2f} ms over {blk} blocks')
        seg_renders = [r for r in renders if (lo['t'] if lo else 0) < r[0] <= hi['t'] and r[2] == 'input-dirty']
        if seg_renders:
            us = sorted(r[1] / 1000 for r in seg_renders)
            print(f'      {"input frames (render us)":24s} n={len(us)} median {statistics.median(us):.1f} '
                  f'p90 {us[int(0.9 * (len(us) - 1))]:.1f} max {us[-1]:.1f} ms')
        seg_paces = [p for p in paces if (lo['t'] if lo else 0) < p[0] <= hi['t']]
        if seg_paces:
            print(f'      {"idle0 gap max (cum.)":24s} {seg_paces[-1][1]:.1f} ms  forced {seg_paces[-1][2]}')
        seg_inputs = [i for i in inputs if (lo['t'] if lo else 0) < i[0] <= hi['t']]
        if seg_inputs:
            v = sorted(i[1] / 1000 for i in seg_inputs)
            print(f'      {"INPUT_SERVICE warnings":24s} n={len(v)} median {statistics.median(v):.1f} max {v[-1]:.1f} ms')
    print()
    print('Whole run (cumulative at the last heartbeat):')
    last = beats[-1]
    print(f'  frames {last["n"]}: frame avg {last["frame"]} compose avg {last["cmp"]} TFT avg {last["tft"]} ms; '
          f'viewport frames {last["vp_n"]} TFT avg {last["vp"]}; other frames {last["oth_n"]} TFT avg {last["oth"]}; '
          f'split fill {last["fill"]} xfer {last["xfer"]}; transactions {last["rows"]} = {last["rows"] / last["n"]:.1f} / frame')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'a3-04e1-hw-soak.log')
