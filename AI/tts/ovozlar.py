#!/usr/bin/env python3
"""Namuna ovozlarni ko'rib chiqadi: uzunligi va o'rtacha ovoz balandligi (F0) - ayol/erkak ovozini ajratish uchun.

WSL'da: ~/navoiy/.venv/bin/python ovozlar.py <wav> [<wav> ...]
"""

import sys

import torch
import torchaudio
import torchaudio.functional as F

for yol in sys.argv[1:]:
    audio, sr = torchaudio.load(yol)
    audio = audio.mean(0)
    f0 = F.detect_pitch_frequency(audio.unsqueeze(0), sr, freq_low=70, freq_high=400).flatten()
    f0 = f0[(f0 > 70) & (f0 < 400)]
    med = float(f0.median()) if f0.numel() else 0.0
    taxmin = "ayol/bola" if med > 165 else "erkak" if med > 0 else "?"
    print(f"{yol.rsplit('/', 1)[-1]:24} {audio.numel() / sr:5.1f} s  F0 ~{med:5.0f} Hz  -> {taxmin}")
