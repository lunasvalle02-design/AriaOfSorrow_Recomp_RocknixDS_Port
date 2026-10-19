#!/usr/bin/env python3
"""Builds a gbarecomp input-replay trace (GBARECOMP_INPUT_REPLAY) from a
compact script, for headless testing.

Script tokens (whitespace separated):
    @FRAME            jump the cursor to an absolute frame
    +N                advance the cursor N frames with nothing pressed
    BTN[,BTN..]*N     hold buttons for N frames (default 4), then release
                      for 8 frames. BTN in A B SELECT START RIGHT LEFT UP DOWN R L
    tapBTN*K          tap BTN K times (4 on / 10 off)
"""
import sys

BITS = dict(A=0, B=1, SELECT=2, START=3, RIGHT=4, LEFT=5, UP=6, DOWN=7, R=8, L=9)


def build(script):
    events = [(0, 0x3FF)]
    cur = 0
    for tok in script.split():
        if tok.startswith('@'):
            cur = int(tok[1:])
        elif tok.startswith('+'):
            cur += int(tok[1:])
        elif tok.startswith('tap'):
            name, _, count = tok[3:].partition('*')
            for _ in range(int(count or 1)):
                events.append((cur, 0x3FF & ~(1 << BITS[name])))
                events.append((cur + 4, 0x3FF))
                cur += 14
        else:
            names, _, dur = tok.partition('*')
            mask = 0
            for n in names.split(','):
                mask |= 1 << BITS[n]
            events.append((cur, 0x3FF & ~mask))
            cur += int(dur or 4)
            events.append((cur, 0x3FF))
            cur += 8
    events.sort(key=lambda e: e[0])
    out = ['# gbarecomp-keyinput-v1']
    out += ['%d,0x%03x' % e for e in events]
    return '\n'.join(out) + '\n', cur


if __name__ == '__main__':
    text, end = build(open(sys.argv[1]).read() if len(sys.argv) > 1 else sys.stdin.read())
    sys.stdout.write(text)
    sys.stderr.write('script ends at frame %d\n' % end)
