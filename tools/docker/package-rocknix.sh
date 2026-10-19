#!/usr/bin/env bash
# Assembles the ROCKNIX ports package (no ROM, no recompiled code beyond the
# binary built from YOUR ROM — keep the result for personal use).
set -euo pipefail
OUT=/src/out/rocknix
rm -rf "$OUT"
mkdir -p "$OUT/ports/ariaofsorrowds/fonts"
cp "/src/packaging/rocknix/Aria of Sorrow DS.sh" "$OUT/ports/"
chmod +x "$OUT/ports/Aria of Sorrow DS.sh"
cp /build/rocknix/aria_recomp.stripped "$OUT/ports/ariaofsorrowds/aria_recomp"
chmod +x "$OUT/ports/ariaofsorrowds/aria_recomp"
cp /src/packaging/rocknix/README.txt "$OUT/ports/ariaofsorrowds/"
cp /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf "$OUT/ports/ariaofsorrowds/fonts/"
cp /usr/share/doc/fonts-dejavu-core/copyright "$OUT/ports/ariaofsorrowds/fonts/DejaVu-LICENSE.txt"
cp /src/third_party/gbarecomp/LICENSE "$OUT/ports/ariaofsorrowds/LICENSE-gbarecomp.txt"
cd "$OUT"
python3 - <<'PY'
import zipfile, os
with zipfile.ZipFile('AriaOfSorrowDS-rocknix-aarch64.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for root, _, files in os.walk('ports'):
        for f in files:
            p = os.path.join(root, f)
            info = zipfile.ZipInfo.from_file(p, p)
            info.external_attr = (0o755 if os.access(p, os.X_OK) else 0o644) << 16
            with open(p, 'rb') as fh:
                z.writestr(info, fh.read(), zipfile.ZIP_DEFLATED)
PY
ls -la "$OUT" "$OUT/ports/ariaofsorrowds"
