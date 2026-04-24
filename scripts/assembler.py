#!/usr/bin/env python3
"""
Assembler for the Baba Is You CPU.

USAGE
-----
    python3 assembler.py <source.asm> [output.schem]

    If output.schem is omitted, writes to <source>.schem alongside the source.
    The schem is designed to overlay directly on schematics/meminst.schem.

ISA QUICK REFERENCE
-------------------
Registers: r0, r1, r2, r3  (8-bit each)

  WARNING: r0 is used as scratch for label-jump expansion.
           Do not rely on r0 being preserved after any 'jz <label>' or 'js <label>'.

No-operand:
  halt          -- stop execution (pc frozen)
  nop           -- no operation   (encoded as 0x09, an unused slot)
  play          -- play stdout buffer as music

One-operand:
  print rt      -- print register rt

Two-operand (reg, reg):
  sub  rt, ra   -- rt = rt - ra
  cpy  rt, ra   -- rt = ra
  jz   ra, rt   -- if ra == 0:  pc = rt
  js   ra, rt   -- if ra <  0:  pc = rt   (sign = bit 7, two's complement)
  ld   rt, ra   -- rt = mem[ra]
  st   ra, rt   -- mem[ra] = rt

Two-operand (reg, immediate):
  addi rt, imm  -- rt = rt + imm  [imm: unsigned 2-bit, range 0..3]
  movl rt, imm  -- set lower nibble of rt to imm  [imm: unsigned 4-bit, 0..15]
  movh rt, imm  -- set upper nibble of rt to imm  [imm: unsigned 4-bit, 0..15]

  ASSUMPTION: movl and movh each set only their nibble and PRESERVE the other.
    Hardware model:  movl → rt = (rt & 0xF0) | (imm & 0x0F)
                     movh → rt = ((imm & 0x0F) << 4) | (rt & 0x0F)
    To load a full byte use both:  movh r0, hi ; movl r0, lo

Label pseudo-instructions (CRACKED → 3 real instructions using r0):
  jz   ra, my_label  →  movh r0, hi4 ; movl r0, lo4 ; jz  ra, r0
  js   ra, my_label  →  movh r0, hi4 ; movl r0, lo4 ; js  ra, r0

  WARNING: using r0 as the condition register (ra) with a label target is almost
           certainly a bug — after cracking, r0 holds the target address, not 0.

Load-address pseudo-instruction (CRACKED → 2 real instructions, any target reg):
  la   rt, my_label  →  movh rt, hi4 ; movl rt, lo4

  Stashes a code address (e.g. the PC of the next instruction) into rt. Put a
  label at the instruction you want to point at, then `la rt, label` earlier.

COMMENTS
--------
  ; semicolon to end of line
  # hash to end of line

LABELS
------
  my_label:              -- define a label at the current address
  my_label: instruction  -- label and instruction on the same line is OK

ENCODING REFERENCE (binary patterns)
-------------------------------------
  00000000  halt
  000001tt  print
  00001000  play
  00001001  nop      (chosen — any "others" slot works)
  0001aatt  sub
  0010iitt  addi     (ii = unsigned 2-bit: 00=0,01=1,10=2,11=3)
  0011aatt  cpy
  01iiiitt  movl     (iiii = lower 4 bits of immediate)
  10iiiitt  movh     (iiii = upper 4 bits to set)
  1100aatt  jz
  1101aatt  js
  1110aatt  ld
  1111aatt  st

FULL EXAMPLE
------------
  ; Count r1 down from 9 to 0, printing each value, then halt.
  ; r0 = scratch (reserved), r1 = counter, r2 = decrement step (1)

      movl r1, 9      ; r1 = 9  (movh not needed; upper nibble stays 0)
      movl r2, 1      ; r2 = 1

  loop:
      print r1
      sub   r1, r2    ; r1 -= 1
      jz    r1, done  ; if r1 == 0, jump to 'done'  [CRACKED: uses r0]
      movh  r0, 0     ; reset r0 upper nibble to 0 after crack overwrote it
      movl  r0, 0     ; reset r0 lower nibble to 0
      jz    r0, loop  ; unconditional: r0 == 0 → always taken

  done:
      halt
"""

import sys
import os

# ---------------------------------------------------------------------------
# Hex nibble → Baba object name (the interleaved memory alphabet)
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# Schem grid layout (derived from tile positions in schematics/meminst.schem)
#   Left  block (MSB nibbles): x in [-91, -76], y in [42, 57]
#   Right block (LSB nibbles): x in [-71, -56], y in [42, 57]
#   Instruction N → row = N // 16, col = N % 16
# ---------------------------------------------------------------------------
LEFT_X      = -91
RIGHT_X     = -71
GRID_Y      = 42
GRID_COLS   = 16
GRID_ROWS   = 16
MAX_BYTES   = GRID_COLS * GRID_ROWS   # 256

# ---------------------------------------------------------------------------
# Register encoding
# ---------------------------------------------------------------------------
REGISTERS = {'r0': 0b00, 'r1': 0b01, 'r2': 0b10, 'r3': 0b11}


# ---------------------------------------------------------------------------
# Error type
# ---------------------------------------------------------------------------
class AssemblyError(Exception):
    pass


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def parse_reg(token, line_no):
    t = token.lower()
    if t not in REGISTERS:
        raise AssemblyError(f"line {line_no}: invalid register '{token}' (use r0–r3)")
    return REGISTERS[t]


def parse_imm(token, lo, hi, line_no, label='immediate'):
    try:
        v = int(token, 0)   # accepts decimal, 0x hex, 0b binary, 0o octal
    except ValueError:
        raise AssemblyError(f"line {line_no}: expected {label}, got '{token}'")
    if not (lo <= v <= hi):
        raise AssemblyError(
            f"line {line_no}: {label} {v} out of range ({lo}..{hi})"
        )
    return v


# ---------------------------------------------------------------------------
# Tokenizer
# ---------------------------------------------------------------------------
def tokenize(source):
    """Yield (line_no, [token, ...]) for every non-empty, non-comment line."""
    for line_no, raw in enumerate(source.splitlines(), 1):
        line = raw.split(';')[0].split('#')[0].strip()
        if not line:
            continue
        yield line_no, line.replace(',', ' ').split()


# ---------------------------------------------------------------------------
# First pass — build label → address table
#
# Label jumps (jz/js to a label) are fixed at 3 instructions each so that
# all addresses are deterministic after a single pass.
# ---------------------------------------------------------------------------
def instruction_size(tokens):
    """Return 3 for a label-targeting jz/js, 2 for la, 1 for everything else."""
    if len(tokens) >= 3 and tokens[0].lower() in ('jz', 'js'):
        if tokens[2].lower() not in REGISTERS:
            return 3
    if tokens[0].lower() == 'la':
        return 2
    return 1


def first_pass(lines):
    label_table = {}
    label_lines = {}
    pc = 0

    for line_no, tokens in lines:
        if tokens[0].endswith(':'):
            name = tokens[0][:-1]
            if not name:
                raise AssemblyError(f"line {line_no}: empty label name")
            if name in label_table:
                raise AssemblyError(
                    f"line {line_no}: duplicate label '{name}' "
                    f"(first defined at line {label_lines[name]})"
                )
            label_table[name] = pc
            label_lines[name] = line_no
            tokens = tokens[1:]     # remainder after label
            if not tokens:
                continue

        size = instruction_size(tokens)
        pc += size
        if pc > MAX_BYTES:
            raise AssemblyError(
                f"line {line_no}: program exceeds {MAX_BYTES}-byte limit "
                f"(reached {pc} bytes)"
            )

    return label_table


# ---------------------------------------------------------------------------
# Encode one logical instruction → list of (asm_str, byte) pairs
# ---------------------------------------------------------------------------
def encode(tokens, line_no, label_table):
    op = tokens[0].lower()

    def expect(n):
        got = len(tokens) - 1
        if got != n:
            raise AssemblyError(
                f"line {line_no}: '{op}' expects {n} operand(s), got {got}"
            )

    # --- No-operand ---
    if op == 'halt':
        expect(0)
        return [('halt', 0x00)]

    if op == 'nop':
        expect(0)
        return [('nop', 0x09)]

    if op == 'play':
        expect(0)
        return [('play', 0x08)]

    # --- print rt ---
    if op == 'print':
        expect(1)
        tt = parse_reg(tokens[1], line_no)
        return [(f'print r{tt}', 0x04 | tt)]

    # --- sub rt, ra ---
    if op == 'sub':
        expect(2)
        tt = parse_reg(tokens[1], line_no)
        aa = parse_reg(tokens[2], line_no)
        return [(f'sub r{tt}, r{aa}', 0x10 | (aa << 2) | tt)]

    # --- addi rt, imm2 (unsigned 0..3) ---
    if op == 'addi':
        expect(2)
        tt  = parse_reg(tokens[1], line_no)
        imm = parse_imm(tokens[2], 0, 3, line_no, 'imm2 (0..3)')
        return [(f'addi r{tt}, {imm}', 0x20 | (imm << 2) | tt)]

    # --- cpy rt, ra ---
    if op == 'cpy':
        expect(2)
        tt = parse_reg(tokens[1], line_no)
        aa = parse_reg(tokens[2], line_no)
        return [(f'cpy r{tt}, r{aa}', 0x30 | (aa << 2) | tt)]

    # --- movl rt, imm4 (0..15) ---
    if op == 'movl':
        expect(2)
        tt  = parse_reg(tokens[1], line_no)
        imm = parse_imm(tokens[2], 0, 15, line_no, 'imm4 (0..15)')
        return [(f'movl r{tt}, {imm}', 0x40 | (imm << 2) | tt)]

    # --- movh rt, imm4 (0..15) ---
    if op == 'movh':
        expect(2)
        tt  = parse_reg(tokens[1], line_no)
        imm = parse_imm(tokens[2], 0, 15, line_no, 'imm4 (0..15)')
        return [(f'movh r{tt}, {imm}', 0x80 | (imm << 2) | tt)]

    # --- la rt, label (load address; cracks to movh + movl) ---
    if op == 'la':
        expect(2)
        tt = parse_reg(tokens[1], line_no)
        target = tokens[2]
        if target not in label_table:
            raise AssemblyError(f"line {line_no}: undefined label '{target}'")
        addr = label_table[target]
        hi4  = (addr >> 4) & 0xF
        lo4  = addr & 0xF
        movh_byte = 0x80 | (hi4 << 2) | tt
        movl_byte = 0x40 | (lo4 << 2) | tt
        return [
            (f'movh r{tt}, {hi4}   ; la: addr {addr:#04x} of {target}',
             movh_byte),
            (f'movl r{tt}, {lo4}',
             movl_byte),
        ]

    # --- jz / js ---
    if op in ('jz', 'js'):
        expect(2)
        aa = parse_reg(tokens[1], line_no)
        target = tokens[2]

        # Register target (direct form — no cracking)
        if target.lower() in REGISTERS:
            tt   = parse_reg(target, line_no)
            base = 0xC0 if op == 'jz' else 0xD0
            return [(f'{op} r{aa}, r{tt}', base | (aa << 2) | tt)]

        # Label target — crack into movh + movl + jz/js using r0 as scratch
        if target not in label_table:
            raise AssemblyError(f"line {line_no}: undefined label '{target}'")

        addr = label_table[target]
        hi4  = (addr >> 4) & 0xF
        lo4  = addr & 0xF

        if aa == 0b00:
            print(
                f"  WARNING line {line_no}: '{op} r0, {target}' uses r0 as the "
                f"condition register, but r0 is overwritten by the crack with addr "
                f"{addr:#04x} — this will almost certainly not jump as intended.",
                file=sys.stderr,
            )

        movh_byte = 0x80 | (hi4 << 2) | 0b00
        movl_byte = 0x40 | (lo4 << 2) | 0b00
        jmp_base  = 0xC0 if op == 'jz' else 0xD0
        jmp_byte  = jmp_base | (aa << 2) | 0b00

        return [
            (f'movh r0, {hi4}   ; crack: addr {addr:#04x} for {op} r{aa}, {target}',
             movh_byte),
            (f'movl r0, {lo4}',
             movl_byte),
            (f'{op} r{aa}, r0',
             jmp_byte),
        ]

    # --- ld rt, ra ---
    if op == 'ld':
        expect(2)
        tt = parse_reg(tokens[1], line_no)
        aa = parse_reg(tokens[2], line_no)
        return [(f'ld r{tt}, r{aa}', 0xE0 | (aa << 2) | tt)]

    # --- st ra, rt ---
    if op == 'st':
        expect(2)
        aa = parse_reg(tokens[1], line_no)
        tt = parse_reg(tokens[2], line_no)
        return [(f'st r{aa}, r{tt}', 0xF0 | (aa << 2) | tt)]

    raise AssemblyError(f"line {line_no}: unknown instruction '{op}'")


# ---------------------------------------------------------------------------
# Full assembly pipeline
# ---------------------------------------------------------------------------
def assemble(source):
    lines = list(tokenize(source))

    label_table = first_pass(lines)

    cracked = []    # list of (asm_str | None, byte | None)
    raw_bytes = []

    for line_no, tokens in lines:
        if tokens[0].endswith(':'):
            cracked.append((f'{tokens[0][:-1]}:', None))
            tokens = tokens[1:]
            if not tokens:
                continue

        pairs = encode(tokens, line_no, label_table)
        cracked.extend(pairs)
        raw_bytes.extend(b for _, b in pairs)

    if len(raw_bytes) > MAX_BYTES:
        raise AssemblyError(
            f"program is {len(raw_bytes)} bytes — exceeds {MAX_BYTES}-byte limit"
        )

    # Pad remainder with halt (0x00)
    raw_bytes += [0x00] * (MAX_BYTES - len(raw_bytes))
    return label_table, cracked, raw_bytes


# ---------------------------------------------------------------------------
# Output: .schem file
# ---------------------------------------------------------------------------
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
        hi4  = (byte >> 4) & 0xF
        lo4  = byte & 0xF
        lx   = LEFT_X  + col
        rx   = RIGHT_X + col
        gy   = GRID_Y  + row
        lines.append(f'object {lx} {gy} {SYMBOLS[hi4]} down')
        lines.append(f'object {rx} {gy} {SYMBOLS[lo4]} down')
    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Output: text preview of both blocks
# ---------------------------------------------------------------------------
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

    print('\nLeft block (MSB nibbles — upper 4 bits of each instruction):')
    for r in render_block(msb_side=True):
        print(' ', r)

    print('\nRight block (LSB nibbles — lower 4 bits of each instruction):')
    for r in render_block(msb_side=False):
        print(' ', r)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    if len(sys.argv) < 2:
        print(f'Usage: {sys.argv[0]} <source.asm> [output.schem]', file=sys.stderr)
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
        label_table, cracked, raw_bytes = assemble(source)
    except AssemblyError as e:
        print(f'AssemblyError: {e}', file=sys.stderr)
        sys.exit(1)

    # --- Label table ---
    if label_table:
        print('Labels:')
        for name, addr in sorted(label_table.items(), key=lambda kv: kv[1]):
            print(f'  {name:20s}  addr {addr:3d}  (0x{addr:02X})')

    # --- Cracked assembly ---
    print('\nCracked assembly:')
    pc = 0
    for asm_str, byte in cracked:
        if byte is None:
            print(f'  {asm_str}')
        else:
            print(f'  {pc:3d} (0x{pc:02X})  {byte:08b}  {asm_str}')
            pc += 1

    code_bytes = sum(1 for b in raw_bytes if b != 0x00)
    print(f'\n  {pc} instruction bytes  ({MAX_BYTES - pc} bytes padded with halt)')

    # --- Text preview ---
    print_grid_preview(raw_bytes)

    # --- Write .schem ---
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
