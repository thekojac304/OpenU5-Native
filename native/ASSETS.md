# Ultima V native asset pack (Milestone 3)

Milestone 3 adds a deterministic host-side converter and a streaming ESP32
validator. It does not render, move the party, or add game rules. The generated
pack contains copyrighted Ultima V data and is intentionally ignored by Git.

## Required original files

Place these four files from a DOS Ultima V installation in
`original/u5/ultima5/` (the existing extractor convention):

- `TILES.16` — LZW-compressed original 512-tile bank
- `BRIT.DAT` — sparse Britannia surface chunks
- `DATA.OVL` — includes the sparse-chunk index at `0x3886`
- `INIT.GAM` — canonical new-game state/template

Names are case-sensitive on case-sensitive development hosts. Nothing is
fabricated when a file is missing: the tool prints the complete missing-file
list and exits without producing a pack. `UNDER.DAT` is deliberately excluded.
It is easy to encode using the existing dense-map parser, but is not needed for
the first surface-world screen and would add another 64 KiB plus scope.

## Generate and validate

Install the repository's normal npm dependencies, then run from the repository
root:

```sh
npm run pack:native
npm run pack:native -- --validate native/assets/openu5-assets.bin
```

Custom paths are supported:

```sh
npm run pack:native -- --source /path/to/ultima5 --output /tmp/openu5-assets.bin
```

For identical four input files and tool version, output is byte-identical. The
default output is `native/assets/openu5-assets.bin`. Copy it to this path on the
FAT-formatted microSD card:

```text
/ultima5/openu5-assets.bin
```

The mounted ESP-IDF path is `/sd/ultima5/openu5-assets.bin`. On boot, the
firmware preserves the Milestone 2 display/SD/shared-SPI checks, then opens this
file. A missing or invalid asset file is reported without rebooting.

## Why indexed 4-bit tiles

The authoritative `extractor/src/parsers/tiles.ts` parser expands `TILES.16`
and validates each pixel against the canonical 16-color EGA palette. The packer
then writes two palette indices per byte (high nibble is the left pixel), which
is the original efficient 128-byte-per-tile organization. The palette is stored
once as 16 RGB565 values. This uses 65,568 bytes for all tile graphics and the
palette; RGB565 tile pixels would use 262,144 bytes. A future renderer can read
one 128-byte tile and expand its nibbles through the already display-native
RGB565 palette with little CPU or RAM cost.

Britannia is 65,536 bytes: a flat row-major 256x256 array with one exact tile ID
per cell. Direct access is `map_offset + y * map_width + x`.

No extra tile-property table is needed for a static screen. The only render
metadata beyond dimensions/formats is the initial record: `location`, `floor`,
`x`, and `y` come from the existing `parseSaveGame(INIT.GAM)` path;
`transport_tile` is read from the existing INIT.GAM field at `0x2D6`; and the
initial on-foot Avatar sprite is tile `0x11C`, matching `CoreView`'s established
`AVATAR_TILE`. There are no code-applied terrain overrides required merely to
display that initial static surface view.

## Binary format v1.0

All multi-byte integers are unsigned, fixed-width, and **little-endian**. Byte
offsets are absolute from the start of the file. The fixed header is 128 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0x00` | 8 | Magic `OU5PACK\0` |
| `0x08` | 2+2 | Major, minor version (`1`, `0`) |
| `0x0C` | 2+2 | Header size (`128`), section count (`4`) |
| `0x10` | 4 | Exact total file size |
| `0x14` | 4 | CRC32 of every byte after the 128-byte header |
| `0x18` | 4 | Flags (zero in v1) |
| `0x1C` | 2+2+2+2 | Tile count, width, height, format |
| `0x24` | 2+2+2+2 | Map width, height, format, reserved |
| `0x2C` | 4 | Reserved (zero) |
| `0x30` | 80 | Four 20-byte section descriptors |

Each descriptor is `type:u32, offset:u32, length:u32, count:u32, crc32:u32`.
Section types and v1 payloads are:

| Type | Payload | Length | Count |
|---:|---|---:|---:|
| 1 | 16-entry RGB565 palette, little-endian words | 32 | 16 |
| 2 | 512 indexed-4bpp tiles, 128 bytes each | 65,536 | 512 |
| 3 | Britannia tile IDs, row-major u8 | 65,536 | 65,536 |
| 4 | Initial-view record | 8 | 1 |

The initial-view record is
`location:u8, floor:u8, x:u8, y:u8, transport_tile:u16, avatar_tile:u16`.
`floor` preserves the raw byte (`0xFF` is the Underworld convention).

The expected v1 file length is 131,240 bytes. Every section has its own standard
IEEE CRC32, and the header carries a second CRC32 over the complete payload.
The ESP32 validates magic, exact version, dimensions/formats, physical file
size, all ranges and overlaps, every section CRC, and the payload CRC. It then
reads tile 0 and the map byte at the initial coordinates as sanity samples. A
format change that is not backward-compatible must increment the major version.
