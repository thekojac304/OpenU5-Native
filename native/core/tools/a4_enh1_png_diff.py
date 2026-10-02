"""A4-ENH1: pixel diff of two a4_ui1_chrome_runtime --dump directories.

    python native/core/tools/a4_enh1_png_diff.py <before_dir> <after_dir>

For every state PNG the --dump wrote, prints "identical" or the count and
bounding box of the differing pixels -- the proof that a golden re-record
changed only the rows the batch meant to change.
"""
import os
import struct
import sys
import zlib


def read_png(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path
    i, idat = 8, b''
    while i < len(data):
        n = struct.unpack('>I', data[i:i + 4])[0]
        kind, chunk = data[i + 4:i + 8], data[i + 8:i + 8 + n]
        i += 12 + n
        if kind == b'IHDR':
            w, h, _depth, colour = struct.unpack('>IIBB', chunk[:10])
        elif kind == b'IDAT':
            idat += chunk
    raw = zlib.decompress(idat)
    bpp = {2: 3, 6: 4}.get(colour, 1)
    stride, rows, prev, o = w * bpp, [], bytearray(w * bpp), 0
    for _ in range(h):
        f = raw[o]
        o += 1
        line = bytearray(raw[o:o + stride])
        o += stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line))
        prev = line
    return w, h, bpp, rows


def main(before, after):
    for name in sorted(os.listdir(before)):
        if not name.endswith('.png'):
            continue
        w, h, bpp, a = read_png(os.path.join(before, name))
        _, _, _, b = read_png(os.path.join(after, name))
        diff = [(x, y) for y in range(h) for x in range(w)
                if a[y][x * bpp:(x + 1) * bpp] != b[y][x * bpp:(x + 1) * bpp]]
        if not diff:
            print('%-20s identical' % name[:-4])
            continue
        xs = [d[0] for d in diff]
        ys = [d[1] for d in diff]
        print('%-20s %5d px differ, x %d..%d y %d..%d' % (name[:-4], len(diff), min(xs), max(xs), min(ys), max(ys)))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
