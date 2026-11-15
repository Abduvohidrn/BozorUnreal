#!/usr/bin/env bash
# gaplar_yoz.py dagi STT tekshiruvini bitta faylda sinaydi
cd "$(dirname "$0")"
"$HOME/navoiy/.venv/bin/python" - <<'EOF'
import time
from pathlib import Path
import gaplar_yoz as g
f = sorted(Path("gaplar").glob("G_*_ayol_xotirjam.wav"))[0]
matn = dict(q.split("\t") for q in Path("gaplar/royxat.tsv").read_text(encoding="utf-8").splitlines())[f.stem]
t = time.perf_counter()
e = g.eshit(f)
print(f"{f.stem}: '{matn}' -> '{e}'  o'xshash {g.oxshash(matn, e):.2f}  {time.perf_counter() - t:.2f} s")
EOF
