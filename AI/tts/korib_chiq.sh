#!/usr/bin/env bash
# Namuna ovoz manbalari: CosyVoice repo namunalari (Apache-2.0) va 0-bosqichdagi Common Voice uz kliplari (CC0) - F0 bo'yicha ayol/erkak
cd "$(dirname "$0")"
PY="$HOME/navoiy/.venv/bin/python"
echo "===== CosyVoice/asset"
"$PY" ovozlar.py "$HOME"/navoiy/CosyVoice/asset/*.wav 2>/dev/null
echo "===== AI/poc/audio"
ls ../poc/audio | head -30
"$PY" ovozlar.py $(find ../poc/audio -name '*.wav' | head -20) 2>/dev/null
