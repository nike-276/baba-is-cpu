#!/usr/bin/env python3
"""
Data-memory compiler for the Baba Is You 8-bit CPU.

USAGE
-----
    python3 memcompiler.py <source.mem> [output.schem]

    If output.schem is omitted, writes to <source>.schem alongside the source.
    The schem overlays the data-RAM block in schematics/mem.schem.

INPUT FORMAT
------------
See scripts/parse_mem.py — hex-dump lines like:

    10: 11 00 18 08    ; first four music frames
    80: 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10

GRID LAYOUT
-----------
    Left  block (MSB nibbles): x in [-96, -81], y in [42, 57]
    Right block (LSB nibbles): x in [-76, -61], y in [42, 57]
    Address N  → row = N // 16, col = N % 16

Nibble alphabet matches the assembler (donut..fruit for 0x0..0xf).
"""

import os
import sys

from parse_mem import parse_mem, MemParseError, MEM_SIZE

# Nibble -> Baba object name. Matches SYMBOLS in assembler.py.
SYMBOLS = {
    0x0: 'donut',
    0x1: 'stick',
    0x2: 'scissors',
    0x3: 'bubble',
    0x4: 'dust',
    0x5: 'hand',
    0x6: 'sax',
    0x7: 'cash',
    0x8: 'cog',
    0x9: 'cat',
    0xA: 'algae',
    0xB: 'bottle',
    0xC: 'cake',
    0xD: 'drink',
    0xE: 'egg',
    0xF: 'fruit',
}

LEFT_X    = -96
RIGHT_X   = -76
GRID_Y    = 42
GRID_COLS = 16
GRID_ROWS = 16


def emit_schem(raw_bytes, out_path):
    lines = [
        'version 1',
        f'name "{out_path}"',
        f'origin {LEFT_X} {GRID_Y}',
        '',
    ]
    for idx, byte in enumerate(raw_bytes):
        row = idx // GRID_COLS
        col = idx % GRID_COLS
        hi4 = (byte >> 4) & 0xF
        lo4 = byte & 0xF
        lx  = LEFT_X  + col
        rx  = RIGHT_X + col
        gy  = GRID_Y  + row
        lines.append(f'object {lx} {gy} {SYMBOLS[hi4]} down')
        lines.append(f'object {rx} {gy} {SYMBOLS[lo4]} down')
    return '\n'.join(lines)


def print_grid_preview(raw_bytes):
    col_w = max(len(s) for s in SYMBOLS.values()) + 2

    def render_block(msb_side):
        rows = []
        for row in range(GRID_ROWS):
            cells = []
            for col in range(GRID_COLS):
                byte = raw_bytes[row * GRID_COLS + col]
                nib  = (byte >> 4) & 0xF if msb_side else byte & 0xF
                cells.append(SYMBOLS[nib].ljust(col_w))
            rows.append(''.join(cells))
        return rows

    print('\nLeft block (MSB nibbles):')
    for r in render_block(msb_side=True):
        print(' ', r)
    print('\nRight block (LSB nibbles):')
    for r in render_block(msb_side=False):
        print(' ', r)


def main():
    if len(sys.argv) < 2:
        print(f'Usage: {sys.argv[0]} <source.mem> [output.schem]', file=sys.stderr)
        sys.exit(1)

    src_path = sys.argv[1]
    out_path = (
        sys.argv[2] if len(sys.argv) > 2
        else os.path.splitext(src_path)[0] + '.schem'
    )

    try:
        with open(src_path) as f:
            source = f.read()
    except OSError as e:
        print(f'Error reading {src_path}: {e}', file=sys.stderr)
        sys.exit(1)

    try:
        raw_bytes = parse_mem(source)
    except MemParseError as e:
        print(f'MemParseError: {e}', file=sys.stderr)
        sys.exit(1)

    nonzero = sum(1 for b in raw_bytes if b != 0)
    print(f'{nonzero} non-zero bytes  ({MEM_SIZE - nonzero} zero)')

    print_grid_preview(raw_bytes)

    schem_text = emit_schem(raw_bytes, out_path)
    try:
        with open(out_path, 'w') as f:
            f.write(schem_text)
        print(f'\nWrote {out_path}')
    except OSError as e:
        print(f'Error writing {out_path}: {e}', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
