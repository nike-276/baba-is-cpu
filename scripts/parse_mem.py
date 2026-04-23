#!/usr/bin/env python3
"""
Parser for .mem hex-dump files used by the Baba Is You 8-bit CPU.

Format
------
    <addr_hex>: <byte_hex> <byte_hex> ...

  - Addresses and bytes are 2-digit hex (no 0x prefix).
  - Lines may skip addresses; unspecified bytes default to 0x00.
  - `;` or `#` starts a comment to end-of-line.
  - Blank lines are ignored.

Example
-------
    00: 00 00 00 00
    10: 11 00 18 08    ; first four music frames
    80: 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10

Public API
----------
    parse_mem(source: str) -> list[int]    # length 256
    MemParseError
"""

MEM_SIZE = 256


class MemParseError(Exception):
    pass


def parse_mem(source):
    mem = [0x00] * MEM_SIZE
    written = [False] * MEM_SIZE

    for line_no, raw in enumerate(source.splitlines(), 1):
        line = raw.split(';')[0].split('#')[0].strip()
        if not line:
            continue

        if ':' not in line:
            raise MemParseError(
                f"line {line_no}: expected '<addr>: <bytes...>', got '{raw.strip()}'"
            )

        addr_str, rest = line.split(':', 1)
        addr_str = addr_str.strip()
        try:
            addr = int(addr_str, 16)
        except ValueError:
            raise MemParseError(
                f"line {line_no}: bad address '{addr_str}' (expected hex)"
            )
        if not (0 <= addr < MEM_SIZE):
            raise MemParseError(
                f"line {line_no}: address 0x{addr:x} out of range (0..0xff)"
            )

        byte_tokens = rest.split()
        if not byte_tokens:
            raise MemParseError(
                f"line {line_no}: address 0x{addr:02x} has no bytes"
            )

        for i, tok in enumerate(byte_tokens):
            cur = addr + i
            if cur >= MEM_SIZE:
                raise MemParseError(
                    f"line {line_no}: byte #{i} would land at 0x{cur:x} "
                    f"(past end of 256-byte memory)"
                )
            try:
                v = int(tok, 16)
            except ValueError:
                raise MemParseError(
                    f"line {line_no}: bad byte '{tok}' (expected 2-digit hex)"
                )
            if not (0 <= v <= 0xff):
                raise MemParseError(
                    f"line {line_no}: byte 0x{v:x} out of range (0..0xff)"
                )
            if written[cur]:
                raise MemParseError(
                    f"line {line_no}: address 0x{cur:02x} already set earlier"
                )
            mem[cur] = v
            written[cur] = True

    return mem


if __name__ == '__main__':
    import sys
    if len(sys.argv) != 2:
        print(f'Usage: {sys.argv[0]} <file.mem>', file=sys.stderr)
        sys.exit(1)
    with open(sys.argv[1]) as f:
        try:
            mem = parse_mem(f.read())
        except MemParseError as e:
            print(f'MemParseError: {e}', file=sys.stderr)
            sys.exit(1)
    for row in range(16):
        cells = ' '.join(f'{mem[row * 16 + c]:02x}' for c in range(16))
        print(f'{row * 16:02x}: {cells}')
