"""Helpers for the raw memory images written by GBARECOMP_DUMP_MEM."""
import struct
LAYOUT = [('iwram', 0x03000000, 32*1024), ('ewram', 0x02000000, 256*1024), ('oam', 0x07000000, 1024),
          ('io', 0x04000000, 1024), ('vram', 0x06000000, 96*1024), ('pal', 0x05000000, 1024)]

class Mem:
    def __init__(self, path):
        data = open(path, 'rb').read()
        self.r = {}
        off = 0
        for name, base, size in LAYOUT:
            self.r[name] = (base, data[off:off+size]); off += size
        import os
        rom = os.path.join(os.path.dirname(__file__), '..', '..', 'rom',
                           'Castlevania - Aria of Sorrow (USA).gba')
        if os.path.exists(rom):
            self.r['rom'] = (0x08000000, open(rom, 'rb').read())
    def region(self, name): return self.r[name][1]
    def _find(self, addr):
        for name, (base, d) in self.r.items():
            if base <= addr < base + len(d): return d, addr - base
        raise KeyError(hex(addr))
    def u8(self, a): d, o = self._find(a); return d[o]
    def u16(self, a): d, o = self._find(a); return struct.unpack_from('<H', d, o)[0]
    def s16(self, a): d, o = self._find(a); return struct.unpack_from('<h', d, o)[0]
    def u32(self, a): d, o = self._find(a); return struct.unpack_from('<I', d, o)[0]
    def s32(self, a): d, o = self._find(a); return struct.unpack_from('<i', d, o)[0]
