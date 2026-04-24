#!/usr/bin/env python3
"""
Software emulator for the Baba Is You 8-bit CPU.

USAGE
-----
    python3 emulator.py <program.asm> <data.mem> [--trace] [--max-cycles N]

Runs the program (.asm assembled in-memory) against a 256-byte data RAM
loaded from .mem. Halts on `halt`, `play` (which first flushes the print
buffer as space-separated hex), or a cycle cap (default 1,000,000).

I/O semantics
-------------
  print rt   -- appends regs[rt] to an internal buffer
  play       -- prints the buffer to stdout as 2-digit hex, space-separated,
                then stops. No audio.

See scripts/assembler.py for the ISA & encoding.
"""

import sys

from assembler import assemble, AssemblyError
from parse_mem import parse_mem, MemParseError


MNEMONICS_2OP_REG = {0x10: 'sub', 0x30: 'cpy', 0xC0: 'jz',
                     0xD0: 'js',  0xE0: 'ld', 0xF0: 'st'}


def decode_mnemonic(op):
    """Return a short human string for the opcode byte (for --trace)."""
    tt = op & 0b11
    aa = (op >> 2) & 0b11

    if op == 0x00:            return 'halt'
    if op == 0x08:            return 'play'
    if op == 0x09:            return 'nop'
    if (op & 0xFC) == 0x04:   return f'print r{tt}'
    if (op & 0xF0) == 0x20:
        imm = (op >> 2) & 0b11
        return f'addi r{tt}, {imm}'
    if (op & 0xC0) == 0x40:
        imm = (op >> 2) & 0b1111
        return f'movl r{tt}, {imm}'
    if (op & 0xC0) == 0x80:
        imm = (op >> 2) & 0b1111
        return f'movh r{tt}, {imm}'
    hi = op & 0xF0
    if hi in MNEMONICS_2OP_REG:
        return f'{MNEMONICS_2OP_REG[hi]} r{aa}, r{tt}'
    return f'??? 0x{op:02x}'


def run(irom, dram, *, trace=False, max_cycles=1_000_000, out=sys.stdout):
    regs = [0, 0, 0, 0]
    pc = 0
    halted = False
    print_buf = []
    reason = 'cycle-cap'
    cycles = 0

    while cycles < max_cycles:
        op = irom[pc]
        tt = op & 0b11
        aa = (op >> 2) & 0b11

        if trace:
            print(
                f'  {cycles:6d}  pc={pc:02x}  op={op:08b}  '
                f'{decode_mnemonic(op):<18s}  '
                f'r=[{regs[0]:02x} {regs[1]:02x} {regs[2]:02x} {regs[3]:02x}]',
                file=sys.stderr,
            )

        jumped = False

        # halt
        if op == 0x00:
            halted = True
            reason = 'halt'
            break

        # play
        elif op == 0x08:
            out.write(' '.join(f'{b:02x}' for b in print_buf))
            out.write('\n')
            out.flush()
            halted = True
            reason = 'play'
            break

        # nop
        elif op == 0x09:
            pass

        # print rt   (000001tt)
        elif (op & 0xFC) == 0x04:
            print_buf.append(regs[tt])

        # sub rt, ra   (0001aatt)
        elif (op & 0xF0) == 0x10:
            regs[tt] = (regs[tt] - regs[aa]) & 0xFF

        # addi rt, imm2   (0010iitt)  — unsigned 0..3
        elif (op & 0xF0) == 0x20:
            imm = (op >> 2) & 0b11
            regs[tt] = (regs[tt] + imm) & 0xFF

        # cpy rt, ra   (0011aatt)
        elif (op & 0xF0) == 0x30:
            regs[tt] = regs[aa]

        # movl rt, imm4   (01iiiitt)
        elif (op & 0xC0) == 0x40:
            imm = (op >> 2) & 0b1111
            regs[tt] = (regs[tt] & 0xF0) | imm

        # movh rt, imm4   (10iiiitt)
        elif (op & 0xC0) == 0x80:
            imm = (op >> 2) & 0b1111
            regs[tt] = (imm << 4) | (regs[tt] & 0x0F)

        # jz ra, rt   (1100aatt) — note operand order: condition=ra, target=rt
        elif (op & 0xF0) == 0xC0:
            if regs[aa] == 0:
                pc = regs[tt]
                jumped = True

        # js ra, rt   (1101aatt) — condition is sign bit of regs[aa]
        elif (op & 0xF0) == 0xD0:
            if regs[aa] & 0x80:
                pc = regs[tt]
                jumped = True

        # ld rt, ra   (1110aatt)
        elif (op & 0xF0) == 0xE0:
            regs[tt] = dram[regs[aa]]

        # st ra, rt   (1111aatt)
        elif (op & 0xF0) == 0xF0:
            dram[regs[aa]] = regs[tt]

        else:
            raise RuntimeError(f'pc={pc:02x}: unreachable opcode 0x{op:02x}')

        if not jumped:
            pc = (pc + 1) & 0xFF
        cycles += 1

    return {
        'reason':    reason,
        'pc':        pc,
        'regs':      regs,
        'cycles':    cycles,
        'halted':    halted,
        'print_buf': print_buf,
        'dram':      dram,
    }


def main():
    argv = sys.argv[1:]
    trace = False
    max_cycles = 1_000_000

    positional = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == '--trace':
            trace = True
        elif a == '--max-cycles':
            i += 1
            max_cycles = int(argv[i])
        elif a.startswith('--max-cycles='):
            max_cycles = int(a.split('=', 1)[1])
        else:
            positional.append(a)
        i += 1

    if len(positional) != 2:
        print(
            f'Usage: {sys.argv[0]} <program.asm> <data.mem> '
            f'[--trace] [--max-cycles N]',
            file=sys.stderr,
        )
        sys.exit(1)

    asm_path, mem_path = positional

    try:
        with open(asm_path) as f:
            asm_source = f.read()
        _labels, _cracked, irom = assemble(asm_source)
    except (OSError, AssemblyError) as e:
        print(f'Error assembling {asm_path}: {e}', file=sys.stderr)
        sys.exit(1)

    try:
        with open(mem_path) as f:
            dram = parse_mem(f.read())
    except (OSError, MemParseError) as e:
        print(f'Error loading {mem_path}: {e}', file=sys.stderr)
        sys.exit(1)

    result = run(irom, list(dram), trace=trace, max_cycles=max_cycles)

    r = result['regs']
    print(
        f'[{result["reason"]}] cycles={result["cycles"]}  pc={result["pc"]:02x}  '
        f'regs=[r0={r[0]:02x} r1={r[1]:02x} r2={r[2]:02x} r3={r[3]:02x}]  '
        f'print_buf={len(result["print_buf"])} bytes',
        file=sys.stderr,
    )


if __name__ == '__main__':
    main()
