#!/usr/bin/env python3
"""
Base91 decoder using the EXACT alphabet specified by the user.

Alphabet (91 chars):
  ABCDEFGHIJKLMNOPQRSTUVWXYZ
  abcdefghijklmnopqrstuvwxyz
  0123456789
  !#$%&()*+,./:;<=>?@[]^_`{|}~"

NOTE: This differs from the "standard" basE91 alphabet (which uses ' and ").
The decode alphabet is provided explicitly by the task, so we honor it.
"""
import sys

ALPHABET = (
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "!#$%&()*+,./:;<=>?@[]^_`{|}~\""
)

assert len(ALPHABET) == 91, f"alphabet must be 91 chars, got {len(ALPHABET)}"

DECODE_MAP = {c: i for i, c in enumerate(ALPHABET)}


def decode(data: str) -> bytes:
    b = 0
    n = 0
    v = -1
    out = bytearray()
    for ch in data:
        if ch not in DECODE_MAP:
            # skip whitespace / unknown
            continue
        c = DECODE_MAP[ch]
        if v < 0:
            v = c
        else:
            v += c * 91
            b |= v << n
            n += 13 if (v & 8191) > 88 else 14
            while True:
                out.append(b & 255)
                b >>= 8
                n -= 8
                if n < 8:
                    break
            v = -1
    if v + 1:
        out.append((b | v << n) & 255)
    return bytes(out)


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: base91.py <encoded-file|- for stdin>\n")
        sys.exit(2)
    arg = sys.argv[1]
    if arg == "-":
        data = sys.stdin.read()
    else:
        with open(arg, "r", encoding="utf-8") as f:
            data = f.read()
    data = data.strip()
    # write raw bytes to stdout (binary-safe)
    sys.stdout.buffer.write(decode(data))


if __name__ == "__main__":
    main()
