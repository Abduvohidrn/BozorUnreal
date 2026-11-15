#!/usr/bin/env bash
# pyworld o'rinbosari: CosyVoice uni yaml yuklashda import qiladi (cosyvoice.dataset.processor), lekin faqat
# o'qitishda (F0 hisoblash) chaqiradi. Python 3.10 uchun tayyor wheel yo'q, yig'ish esa C++ kompilyator so'raydi.
cd "$HOME/navoiy" && source .venv/bin/activate
SP=$(python -c "import site; print(site.getsitepackages()[0])")
python -c "import pyworld" 2>/dev/null && { echo "pyworld bor"; exit 0; }
cat > "$SP/pyworld.py" <<'EOF'
"""O'rinbosar: haqiqiy pyworld o'rnatilmagan (faqat o'qitish uchun kerak)."""
__version__ = "0-orinbosar"
def __getattr__(nom):
    raise ImportError(f"pyworld.{nom}: haqiqiy pyworld o'rnatilmagan (o'qitish uchun build-essential va pip install pyworld)")
EOF
python -c "import pyworld; print('pyworld', pyworld.__version__)"
