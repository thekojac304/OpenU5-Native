# Ultima V native asset pack (Milestone 4)

The native pack has a deterministic host-side converter and streaming ESP32
validator/reader. Milestone 4 v2 adds only the initial Iolo's Hut floor needed
to reproduce INIT.GAM's actual starting context. The generated pack contains
copyrighted Ultima V data and is intentionally ignored by Git.

## Required original files

Place these five files from a DOS Ultima V installation in
`original/u5/ultima5/` (the existing extractor convention):

- `TILES.16` — LZW-compressed original 512-tile bank
- `BRIT.DAT` — sparse Britannia surface chunks
- `DATA.OVL` — includes the sparse-chunk index at `0x3886`
- `INIT.GAM` — canonical new-game state/template
- `DWELLING.DAT` — source of Iolo's Hut (location 13, floor 0)

Names are case-sensitive on case-sensitive development hosts. Nothing is
fabricated when a file is missing: the tool prints the complete missing-file
list and exits without producing a pack. `UNDER.DAT` is deliberately excluded:
the initial Iolo's Hut screen does not require it, and it would add another
64 KiB plus scope.

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

For identical five input files and tool version, output is byte-identical. The
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
palette; RGB565 tile pixels would use 262,144 bytes. The Milestone 4 renderer
reads one 128-byte tile at a time and expands its nibbles through the already
display-native RGB565 palette with little working RAM.

Britannia is 65,536 bytes: a flat row-major 256x256 array with one exact tile ID
per cell. Direct access is `map_offset + y * map_width + x`. Iolo's Hut is one
1,024-byte row-major 32x32 floor copied from `DWELLING.DAT` offset `0x3000`.

No extra tile-property table is needed for a static screen. Render metadata
includes the initial record: `location`, `floor`,
`x`, and `y` come from the existing `parseSaveGame(INIT.GAM)` path;
`transport_tile` is read from the existing INIT.GAM field at `0x2D6`; and the
initial on-foot Avatar sprite is tile `0x11C`, matching `CoreView`'s established
`AVATAR_TILE`. Canonical INIT.GAM selects location 13, floor 0, x=15, y=15.
Location 13 is Iolo's Hut, not Britannia, so those coordinates index its 32x32
local floor. No code-applied terrain overrides are required for this static view.

## Binary format v2.0

All multi-byte integers are unsigned, fixed-width, and **little-endian**. Byte
offsets are absolute from the start of the file. The fixed header is 148 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0x00` | 8 | Magic `OU5PACK\0` |
| `0x08` | 2+2 | Major, minor version (`2`, `0`) |
| `0x0C` | 2+2 | Header size (`148`), section count (`5`) |
| `0x10` | 4 | Exact total file size |
| `0x14` | 4 | CRC32 of every byte after the 148-byte header |
| `0x18` | 4 | Flags (zero in v2) |
| `0x1C` | 2+2+2+2 | Tile count, width, height, format |
| `0x24` | 2+2+2+2 | Map width, height, format, reserved |
| `0x2C` | 2+2 | Initial local-map width and height (`32`, `32`) |
| `0x30` | 100 | Five 20-byte section descriptors |

Each descriptor is `type:u32, offset:u32, length:u32, count:u32, crc32:u32`.
Section types and v2 payloads are:

| Type | Payload | Length | Count |
|---:|---|---:|---:|
| 1 | 16-entry RGB565 palette, little-endian words | 32 | 16 |
| 2 | 512 indexed-4bpp tiles, 128 bytes each | 65,536 | 512 |
| 3 | Britannia tile IDs, row-major u8 | 65,536 | 65,536 |
| 4 | Initial-view record | 8 | 1 |
| 5 | Initial local map: Iolo's Hut floor 0, row-major u8 | 1,024 | 1,024 |

The initial-view record is
`location:u8, floor:u8, x:u8, y:u8, transport_tile:u16, avatar_tile:u16`.
`floor` preserves the raw byte (`0xFF` is the Underworld convention).

The expected v2 file length is 132,284 bytes. Every section has its own standard
IEEE CRC32, and the header carries a second CRC32 over the complete payload.
The ESP32 validates magic, exact version, dimensions/formats, physical file
size, all ranges and overlaps, every section CRC, and the payload CRC. It then
reads tile 0 and the map byte at the initial coordinates as sanity samples. A
format change that is not backward-compatible must increment the major version.

The palette words use standard RGB565 bit placement: red in bits 15..11, green
in bits 10..5, and blue in bits 4..0. They are little-endian only while stored
in this pack. The native reader reconstructs each numeric word, and the display
driver sends that word most-significant byte first as required by the ST7789.
The panel is configured for RGB color order; no red/blue or byte swap is applied
at any other stage.
