#!/usr/bin/env python3
"""Tiles PNG frames into one labelled contact sheet: montage.py OUT.png IN.png..."""
import sys, os
from PIL import Image, ImageDraw

out, files = sys.argv[1], sorted(sys.argv[2:])
imgs = [Image.open(f).convert('RGB') for f in files]
w = max(i.width for i in imgs); h = max(i.height for i in imgs)
cols = int(os.environ.get('COLS', 5))
rows = (len(imgs) + cols - 1) // cols
sheet = Image.new('RGB', (cols * (w + 4), rows * (h + 16)), (40, 40, 40))
d = ImageDraw.Draw(sheet)
for k, (f, im) in enumerate(zip(files, imgs)):
    x, y = (k % cols) * (w + 4), (k // cols) * (h + 16)
    sheet.paste(im, (x, y + 14))
    d.text((x + 2, y + 1), os.path.basename(f)[2:-4], fill=(255, 255, 0))
sheet.save(out)
