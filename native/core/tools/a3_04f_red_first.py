#!/usr/bin/env python
"""Alpha 3 A3-04F -- RED-first: the census checks against the UNMODIFIED Board.

Swaps in HEAD's (the A3-HF2.1 baseline's) tdeck_board.h, tdeck_board.cpp and
native_renderer.cpp, with only the census's inert draw-call counters re-applied
(six increments and the accessor; a3_04e_pacing_runtime's stream hash is
identical with and without them), builds a3_04f_render_runtime, runs it, and
restores the working files byte for byte and touches them (a restored file is
older than its object: without the touch ninja would keep the baseline build).

Usage: python native/core/tools/a3_04f_red_first.py <build-dir> <log> [<git-rev>]
"""
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
CMAKE = 'C:/Dev/TamaPoke/.build-tools/w64devkit/bin/cmake.exe'
BUILD = os.path.abspath(sys.argv[1])
LOG = sys.argv[2]
REV = sys.argv[3] if len(sys.argv) > 3 else 'HEAD'
RES = os.path.join(ROOT, 'native/assets/openu5-alpha1-resources.bin')
FILES = ['native/targets/tdeck/main/tdeck_board.h', 'native/targets/tdeck/main/tdeck_board.cpp',
         'native/targets/tdeck/main/native_renderer.cpp']

# The census counters only (anchor -> replacement), exactly as A3-04F adds them.
INSTRUMENT = {
    'native/targets/tdeck/main/tdeck_board.h': [
        ('struct BedViewportRect { int x, y, width, height; };',
         'struct BoardDrawCalls {\n    uint32_t fill_rects = 0, rgb565 = 0, text_boxes = 0, metric_text_boxes = 0, sky_strips = 0;\n};\n\n'
         'struct BedViewportRect { int x, y, width, height; };'),
        ('    void set_idle_service(IdleService *service) { idle_ = service; }\n',
         '    void set_idle_service(IdleService *service) { idle_ = service; }\n'
         '    const BoardDrawCalls &draw_calls() const { return draw_calls_; }\n'
         '    void reset_draw_calls() { draw_calls_ = {}; }\n'),
        ('    IdleService *idle_ = nullptr;\n', '    IdleService *idle_ = nullptr;\n    BoardDrawCalls draw_calls_{};\n'),
    ],
    'native/targets/tdeck/main/tdeck_board.cpp': [
        ('    ESP_RETURN_ON_ERROR(set_display_window(x, y, width, height), kTag, "set fill window");',
         '    ++draw_calls_.fill_rects;\n    ESP_RETURN_ON_ERROR(set_display_window(x, y, width, height), kTag, "set fill window");'),
        ('    ESP_RETURN_ON_ERROR(set_display_window(x, y, width, height), kTag, "set RGB565 window");',
         '    ++draw_calls_.rgb565;\n    ESP_RETURN_ON_ERROR(set_display_window(x, y, width, height), kTag, "set RGB565 window");'),
        ('        if(!runes_font)return ESP_ERR_INVALID_ARG;\n',
         '        if(!runes_font)return ESP_ERR_INVALID_ARG;\n        ++draw_calls_.sky_strips;\n'),
        ('    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set scaled RGB565 window");',
         '    ++draw_calls_.rgb565;\n    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set scaled RGB565 window");'),
        ('    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set coherent text window");',
         '    ++draw_calls_.text_boxes;\n    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set coherent text window");'),
        ('    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set metric text window");',
         '    ++draw_calls_.metric_text_boxes;\n    ESP_RETURN_ON_ERROR(set_display_window(x,y,width,height),kTag,"set metric text window");'),
    ],
}


def run(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


def main():
    os.environ['PATH'] = os.path.dirname(CMAKE) + os.pathsep + os.environ.get('PATH', '')
    saved = {f: open(os.path.join(ROOT, f), 'rb').read() for f in FILES}
    lines = ['# A3-04F RED-first: a3_04f_render_runtime against %s\'s Board and rasterizer (+ census counters)' % REV, '']
    try:
        for f in FILES:
            code, text = run(['git', 'show', '%s:%s' % (REV, f)])
            if code:
                sys.exit('git show failed for ' + f)
            eol = '\r\n' if b'\r\n' in saved[f] else '\n'
            text = text.replace('\r\n', '\n')
            for old, new in INSTRUMENT.get(f, []):
                if text.count(old) != 1:
                    sys.exit('anchor not unique in %s: %r' % (f, old[:60]))
                text = text.replace(old, new)
            open(os.path.join(ROOT, f), 'wb').write(text.replace('\n', eol).encode('utf-8'))
        code, out = run([CMAKE, '--build', BUILD, '--target', 'a3_04f_render_runtime'])
        if code:
            lines += ['BUILD FAILED'] + [l for l in out.splitlines() if 'error' in l][:8]
        else:
            code, out = run([os.path.join(BUILD, 'a3_04f_render_runtime.exe'), RES])
            lines += [l for l in out.splitlines() if l and not l.startswith(('[I]', '[W]', '[E]'))]
            lines.append('exit=%d' % code)
    finally:
        for f in FILES:
            open(os.path.join(ROOT, f), 'wb').write(saved[f])
            os.utime(os.path.join(ROOT, f), None)
    code, _ = run([CMAKE, '--build', BUILD, '--target', 'a3_04f_render_runtime'])
    lines.append('')
    lines.append('restored the working Board and rasterizer; rebuild exit=%d' % code)
    open(LOG, 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))


if __name__ == '__main__':
    main()
