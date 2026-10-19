"""Finds Thumb BL call sites targeting given addresses: blscan.py ROM ADDR..."""
import struct, sys
rom = open(sys.argv[1], 'rb').read()
targets = {int(a, 16): [] for a in sys.argv[2:]}
for i in range(0, len(rom) - 4, 2):
    h1, h2 = struct.unpack_from('<HH', rom, i)
    if (h1 & 0xF800) == 0xF000 and (h2 & 0xF800) == 0xF800:
        off = ((h1 & 0x7FF) << 12) | ((h2 & 0x7FF) << 1)
        if off & 0x400000: off -= 0x800000
        t = 0x08000000 + i + 4 + off
        if t in targets: targets[t].append(0x08000000 + i)
for t, sites in targets.items():
    print(hex(t), len(sites), ' '.join(hex(s) for s in sites[:60]))
