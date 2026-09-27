#!/usr/bin/env python
"""A3-04F H-200: hardware before / after, hardware figures only.

Before = the A3-04E.1 11-minute soak (ALPHA3_AUDIO.md section 23.10), as
already split by a3_04f_hw_baseline.py (native/core/a3-04f-hw-baseline.log)
or quoted in section 23.10.1 / 26.1. After = the H-200 on-device report,
transcribed in native/targets/tdeck/a3-04f-hw-h200-report.log.

No host-model number appears here: the model's figures stay in section 26.12.

Usage: a3_04f_hw_compare.py [h200-report] [baseline-log]
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
H200 = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'native/targets/tdeck/a3-04f-hw-h200-report.log'
BASE = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / 'native/core/a3-04f-hw-baseline.log'


def read_h200(path):
    out = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        m = re.match(r'^(\w+) = ([0-9.]+)\s*$', line)
        if m:
            out[m.group(1)] = float(m.group(2))
    return out


def read_baseline(path):
    text = path.read_text(encoding='utf-8')
    m80 = text.split('music 80%', 1)[1].split('music  0%', 1)[0]
    m0 = text.split('music  0%', 1)[1].split('Whole run', 1)[0]
    whole = text.split('Whole run', 1)[1]

    def seg(block, label):
        return float(re.search(re.escape(label) + r'\s+([0-9.]+)', block).group(1))

    b = {
        'frame_avg_m80': seg(m80, 'frame avg'),
        'viewport_incl_full_m80': seg(m80, 'viewport-frame TFT avg'),
        'viewport_incl_full_m0': seg(m0, 'viewport-frame TFT avg'),
        'other_m80': seg(m80, 'other-frame TFT avg'),
        'other_m0': seg(m0, 'other-frame TFT avg'),
        'tft_avg_m80': seg(m80, 'TFT avg'),
        'fill_m80': seg(m80, 'TFT row building avg'),
        'xfer_m80': seg(m80, 'TFT SPI transfer avg'),
        'txns_m80': seg(m80, 'transactions / frame'),
    }
    w = re.search(r'frame avg ([0-9.]+) compose avg ([0-9.]+) TFT avg ([0-9.]+) ms; viewport frames \d+ TFT avg ([0-9.]+); '
                  r'other frames \d+ TFT avg ([0-9.]+); split fill ([0-9.]+) xfer ([0-9.]+); transactions \d+ = ([0-9.]+)', whole)
    (b['frame_avg'], b['compose_avg'], b['tft_avg'], b['viewport_incl_full'], b['other'],
     b['fill'], b['xfer'], b['txns']) = map(float, w.groups())
    return b


def main():
    a = read_h200(H200)
    b = read_baseline(BASE)
    txns_after = a['rows'] / a['gameplay_frames']
    # (metric, before, before source, after, after source)
    rows = [
        ('compose avg (ms)', b['compose_avg'], 'baseline log, whole run', a['compose_avg_ms'], 'compose avg'),
        ('compose max (ms)', 43.3, 'section 26.1', a['compose_max_ms'], 'compose max'),
        ('tiles max (ms)', 37.4, 'section 26.1', a['tiles_max_ms'], 'tiles max'),
        ('full-screen TFT max (ms)', 148.8, 'section 23.10.1', a['full_screen_tft_max_ms'], 'full-screen TFT max'),
        ('full-screen TFT avg (ms)', 147.0, 'section 23.10.1', a['full_screen_tft_avg_ms'], 'full-screen TFT avg'),
        ('viewport w/o full TFT avg (ms)', 51.9, 'section 23.10.1', a['viewport_wo_full_tft_avg_ms'], 'viewport w/o full'),
        ('viewport w/o full TFT max (ms)', 71.2, 'section 23.10.1', a['viewport_wo_full_tft_max_ms'], 'viewport w/o full max'),
        ('viewport incl. full TFT avg (ms)', b['viewport_incl_full'], 'baseline log, whole run', a['viewport_tft_avg_ms'], 'viewport frames'),
        ('  same, music 80 % segment', b['viewport_incl_full_m80'], 'baseline log, music 80 %', a['viewport_tft_avg_ms'], 'viewport frames'),
        ('other (animation) TFT avg (ms)', b['other'], 'baseline log, whole run', a['other_tft_avg_ms'], 'other frames'),
        ('  same, music 80 % segment', b['other_m80'], 'baseline log, music 80 %', a['other_tft_avg_ms'], 'other frames'),
        ('  same, music 0 % segment', b['other_m0'], 'baseline log, music 0 %', a['other_tft_avg_ms'], 'other frames'),
        ('idle0 gap max (ms)', 102.9, 'section 23.10.1', a['idle0_gap_max_ms'], 'idle0 gap max'),
        ('frame avg (ms)', b['frame_avg'], 'baseline log, whole run', a['frame_avg_ms'], 'frame avg'),
        ('  same, music 80 % segment', b['frame_avg_m80'], 'baseline log, music 80 %', a['frame_avg_ms'], 'frame avg'),
        ('frame p95 (ms)', 94.0, 'section 26.1', a['frame_p95_ms'], 'p95'),
        ('frame max (ms)', 186.1, 'section 23.10.1', a['frame_max_ms'], 'frame max'),
        ('TFT avg per frame (ms)', b['tft_avg'], 'baseline log, whole run', a['tft_avg_ms'], 'TFT avg'),
        ('TFT row building per frame (ms)', b['fill'], 'baseline log, whole run', a['tft_fill_per_frame_ms'], 'TFT/frame fill'),
        ('TFT SPI transfer per frame (ms)', b['xfer'], 'baseline log, whole run', a['tft_transfer_per_frame_ms'], 'transfer'),
        ('pixel transactions per frame', b['txns'], 'baseline log, whole run', txns_after, 'rows / frames'),
    ]
    print('A3-04F H-200: hardware before (A3-04E.1 soak) / after (A3-04F report). Hardware only.')
    print(f'after: {H200.relative_to(ROOT).as_posix()}')
    print(f'before: {BASE.relative_to(ROOT).as_posix()} and ALPHA3_AUDIO.md sections 23.10.1 / 26.1')
    print()
    print(f'{"metric":36s} {"before":>9s} {"after":>9s} {"change":>8s}  before source')
    for name, before, src, after, _ in rows:
        print(f'{name:36s} {before:9.1f} {after:9.1f} {100.0 * (after - before) / before:+7.1f} %  {src}')
    print()
    print(f'drawn frames per second: {a["gameplay_frames"] / a["window_s"]:.1f}; '
          f'late > 55 ms: {int(a["late_gt55ms"])} of {int(a["gameplay_frames"])} '
          f'({100.0 * a["late_gt55ms"] / a["gameplay_frames"]:.1f} %); '
          f'loop asleep {100.0 * a["loop_asleep_s"] / a["window_s"]:.1f} % (A3-04E.1: 58 %)')
    # Different window lengths, so a count, not a percentage.
    print(f'slow SPI transactions: {int(a["slow"])} in {a["window_s"]:.1f} s '
          f'(A3-04E.1: 15 over the 11-minute soak, section 23.10.5)')
    print(f'audio: missed {int(a["missed"])} underrun {int(a["underrun"])} hw {int(a["hw"])}; '
          f'render avg {a["render_avg_ms"]:.2f} ms per block (A3-04E.1 music 80 %: 3.21 ms)')
    crit = [
        ('compose avg <= 26 ms', a['compose_avg_ms'] <= 26.0),
        ('full-screen TFT max < 148.8 ms', a['full_screen_tft_max_ms'] < 148.8),
        ('und = hw = miss = 0', a['underrun'] == 0 and a['hw'] == 0 and a['missed'] == 0 and a['miss'] == 0),
        ('idle0 gap max < 250 ms', a['idle0_gap_max_ms'] < 250.0),
    ]
    print()
    for name, ok in crit:
        print(f'H-200 criterion {name:32s} {"PASS" if ok else "FAIL"}')
    return 0 if all(ok for _, ok in crit) else 1


if __name__ == '__main__':
    sys.exit(main())
