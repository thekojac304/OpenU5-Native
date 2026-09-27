#!/usr/bin/env python
"""Alpha 3 A3-04F -- mutation validation of the render / TFT efficiency changes
(ALPHA3_AUDIO.md section 26).

Each mutation changes ONE production anchor in the Board (tdeck_board.{h,cpp})
or the rasterizer (native_renderer.cpp), rebuilds the two host targets that run
the REAL Board over the fake ST7789 -- a3_04f_render_runtime (the census, the
retained-region and transcript rules, the CRC identity, the panel golden) and
a3_04e_pacing_runtime (the pauses, the panel equals the composition) -- and
records which checks turn RED. The file is restored byte for byte and touched
(a restored file is older than the mutated object: without the touch ninja
keeps the mutant). A mutation that leaves every check GREEN is a SURVIVOR and
fails this run; one that does not build is INVALID (not killed). A mutant whose
test crashes (a buffer overrun, say) counts as killed by its exit code.

Usage: python native/core/tools/a3_04f_mutation_check.py <build-dir> <log> [K1,E2,...]
(the optional third argument re-runs only the named mutations)
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
TARGETS = ['a3_04f_render_runtime', 'a3_04e_pacing_runtime']

BOARD = 'native/targets/tdeck/main/tdeck_board.cpp'
BOARD_H = 'native/targets/tdeck/main/tdeck_board.h'
RENDER = 'native/targets/tdeck/main/native_renderer.cpp'

CRC_STEPS = ('        crc = kCrc32Table[(crc ^ pixel) & 0xffU] ^ (crc >> 8);\n'
             '        crc = kCrc32Table[(crc ^ (pixel >> 8)) & 0xffU] ^ (crc >> 8);\n')
TRANSCRIPT_KEY = 'auto&cached=transcript_cache_[i];if(!alpha_ui_cache_valid_||cached.color!=color||std::strcmp(cached.text,text)!=0){'
PANEL_KEY = '    if(!force&&cached.valid&&cached.color==color&&cached.invert==invert&&std::strcmp(cached.text,text)==0)return ESP_OK;'
PANEL_LEARN = '    std::snprintf(cached.text,sizeof(cached.text),"%s",text);cached.color=color;cached.invert=invert;cached.valid=true;'
BATCH = '        return row + 1 == height || used + row_bytes > transfer_row_.size() || openu5::tft_row_yield_due(row);'
RUN = '            int end=col+1;while(end<openu5::kViewportTiles&&animated_cells[row*openu5::kViewportTiles+end])++end;'

MUTATIONS = [
    # -- the viewport CRC ---------------------------------------------------------------
    ('K1 the CRC takes a pixel\'s high byte first', RENDER, CRC_STEPS,
     '        crc = kCrc32Table[(crc ^ (pixel >> 8)) & 0xffU] ^ (crc >> 8);\n'
     '        crc = kCrc32Table[(crc ^ pixel) & 0xffU] ^ (crc >> 8);\n'),
    ('K2 the CRC skips its final inversion', RENDER, '    return crc ^ 0xffffffffU;', '    return crc;'),
    ('K3 the CRC reads only each pixel\'s low byte', RENDER, CRC_STEPS,
     '        crc = kCrc32Table[(crc ^ pixel) & 0xffU] ^ (crc >> 8);\n'),
    # -- fill_rect ---------------------------------------------------------------------
    ('F1 fill chunks stop at the rectangle\'s width again (one per row of a frame line)', BOARD,
     '    const size_t pixels_per_chunk = kDisplayWidth;',
     '    const size_t pixels_per_chunk = std::min(width, kDisplayWidth);'),
    ('F2 320 px chunks from a buffer filled only as wide as the rectangle', BOARD,
     '    for (size_t index = 0; index < pixels_per_chunk; ++index) {',
     '    for (size_t index = 0; index < std::min<size_t>(size_t(width), pixels_per_chunk); ++index) {'),
    # -- whole rows per transaction ------------------------------------------------------
    ('R1 no row packing: every row its own transaction again', BOARD_H, BATCH,
     '        (void)used; (void)row_bytes; (void)row; (void)height; return true;'),
    ('R2 packed rows are built over each other (the batch offset ignored)', BOARD,
     '        uint8_t *out = row_bytes.data() + used;', '        uint8_t *out = row_bytes.data();'),
    ('R3 a batch runs through a pause row (the pause is skipped)', BOARD_H, BATCH,
     '        return row + 1 == height || used + row_bytes > transfer_row_.size();'),
    ('R4 a batch may outgrow the 640 B buffer / max_transfer_sz by a row', BOARD_H, BATCH,
     '        return row + 1 == height || used > transfer_row_.size() || openu5::tft_row_yield_due(row);'),
    # -- animated cells ------------------------------------------------------------------
    ('E1 adjacent animated cells are separate windows again', BOARD, RUN, '            int end=col+1;'),
    ('E2 a run\'s window is one cell wide (the rest of the run is not drawn)', BOARD,
     '(end-col)*openu5::kTilePixels,height,', 'openu5::kTilePixels,height,'),
    # -- retained party / status rows ----------------------------------------------------
    ('B1 every frame redraws all nine party / status rows again', BOARD,
     'draw_party_rows(game,party_highlight,!alpha_ui_cache_valid_)', 'draw_party_rows(game,party_highlight,true)'),
    ('B2 a row is never forced (a painted-over panel keeps its blank rows)', BOARD, PANEL_KEY,
     '    (void)force;if(cached.valid&&cached.color==color&&cached.invert==invert&&std::strcmp(cached.text,text)==0)return ESP_OK;'),
    ('B3 reverse video is not part of a row\'s key', BOARD, PANEL_KEY,
     '    if(!force&&cached.valid&&cached.color==color&&std::strcmp(cached.text,text)==0)return ESP_OK;'),
    ('B5 a forced draw (full repaint, bed refresh) does not teach the row cache', BOARD, PANEL_LEARN,
     '    if(!force){' + PANEL_LEARN.strip() + '}'),
    # -- the transcript --------------------------------------------------------------------
    ('T1 the line\'s sequence number is back in the key (every scrolled row redrawn)', BOARD, TRANSCRIPT_KEY,
     'auto&cached=transcript_cache_[i];if(!alpha_ui_cache_valid_||cached.sequence!=seq||cached.color!=color||std::strcmp(cached.text,text)!=0){'),
    ('T2 the transcript ignores invalidation (a painted-over panel keeps stale rows)', BOARD, TRANSCRIPT_KEY,
     'auto&cached=transcript_cache_[i];if(cached.color!=color||std::strcmp(cached.text,text)!=0){'),
    ('T3 a row\'s colour is not part of its key', BOARD, TRANSCRIPT_KEY,
     'auto&cached=transcript_cache_[i];if(!alpha_ui_cache_valid_||std::strcmp(cached.text,text)!=0){'),
    # -- mode transitions still invalidate -------------------------------------------------
    # Leaving the Developer screen invalidates the retained panel twice: directly,
    # and by forgetting the UI size (alpha_ui_size_cache_=0xff), which forces the
    # right-panel reflow and its own invalidation. The first pass removed only the
    # first and SURVIVED -- an equivalent mutant (a3-04f-mutation.log). M1 removes both.
    ('M1 leaving the Developer screen keeps the retained right panel (both invalidations gone)', BOARD,
     'debug_drawn_=false;debug_cache_valid_=false;alpha_ui_cache_valid_=false;alpha_ui_size_cache_=0xff;',
     'debug_drawn_=false;debug_cache_valid_=false;'),
    ('M2 closing the compact selector (Z) keeps the retained right panel', BOARD,
     '        selection_cache_valid_=false;context_cache_valid_=false;alpha_ui_cache_valid_=false;',
     '        selection_cache_valid_=false;context_cache_valid_=false;'),
]


def run(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def build():
    return run([CMAKE, '--build', BUILD, '--target'] + TARGETS)


def test():
    results = []
    for t in TARGETS:
        code, out = run([os.path.join(BUILD, t + '.exe'), RES])
        reds = [l for l in out.splitlines() if l.startswith('RED')]
        results.append((t, code, reds))
    return results


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    lines = ['# A3-04F mutation validation (%s)' % time.strftime('%Y-%m-%d %H:%M'), '']
    survivors = invalid = 0
    only = set(sys.argv[3].split(',')) if len(sys.argv) > 3 else None
    selected = [m for m in MUTATIONS if not only or m[0].split()[0] in only]
    for label, rel, old, new in selected:
        path = os.path.join(ROOT, rel)
        original = open(path, 'rb').read()
        text = original.decode('utf-8')
        eol = '\r\n' if '\r\n' in text else '\n'
        o, n = old.replace('\n', eol), new.replace('\n', eol)
        count = text.count(o)
        if count != 1:
            lines.append('%s: ANCHOR NOT UNIQUE (%d) in %s' % (label, count, rel))
            invalid += 1
            continue
        try:
            open(path, 'wb').write(text.replace(o, n).encode('utf-8'))
            code, out = build()
            if code != 0:
                lines.append('%s [%s]: BUILD FAILED (INVALID, not killed)' % (label, rel))
                lines += ['    ' + l for l in out.splitlines() if 'error' in l][:4]
                invalid += 1
                continue
            results = test()
        finally:
            open(path, 'wb').write(original)
            os.utime(path, None)
        killed = any(code != 0 for _, code, _ in results)
        survivors += 0 if killed else 1
        lines.append('%s [%s]: %s' % (label, rel, 'KILLED' if killed else 'SURVIVED'))
        for t, code, reds in results:
            lines.append('    %s exit=%d  %d RED' % (t, code, len(reds)))
            lines += ['        ' + r[:200] for r in reds]
    bcode, _ = build()
    restored = test()
    lines.append('')
    lines.append('restored: build exit=%d; %s' % (bcode, ', '.join('%s exit=%d' % (t, c) for t, c, _ in restored)))
    lines.append('mutations: %d, killed: %d, survived: %d, invalid: %d' %
                 (len(selected), len(selected) - survivors - invalid, survivors, invalid))
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    sys.exit(1 if survivors or invalid or bcode or any(c for _, c, _ in restored) else 0)


if __name__ == '__main__':
    main()
