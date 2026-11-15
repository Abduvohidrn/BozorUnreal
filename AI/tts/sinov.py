#!/usr/bin/env python3
"""Navoiy sinovi: modelni bir marta yuklab, sotuvchi gaplarini turli uslubda yozadi va vaqtini o'lchaydi.

WSL'da:  ~/navoiy/.venv/bin/python sinov.py --ref <namuna.wav> --chiqish natija/
Natija: natija/<nn>_<uslub>.wav va jadval (gap uzunligi, yozish vaqti, RTF, VRAM).
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

NAVOIY = Path.home() / "navoiy"

# fe'l -> Navoiy uslubi (emotions_40h.json dagi "uz" nomi): Mehribon - iliq, Qattiq - xotirjam/jahldor,
# Shoshqaloq - qo'rqqan (asabiy, tez), Ayyor - kinoyali
GAPLAR = [
    ("mehribon", "Assalomu alaykum, bolam! Keling, nima olasiz? Pomidor, bodring hammasi yangi."),
    ("mehribon", "Siz odobli bola ekansiz, o'n ming so'mdan beraman."),
    ("xotirjam", "Bir kilo pomidor o'n ikki ming besh yuz so'm."),
    ("jahldor", "Narxi shu. Tushmaydi. Boshqa joyda qimmatroq."),
    ("qo'rqqan", "Ha, tez ayting, nima olasiz? Nechta? Tezroq, bolam."),
    ("kinoyali", "Voy, bundan arzon bersam o'zim zarar qilaman-ku!"),
    ("xotirjam", "Mana, ikki yarim kilo kartoshka. Jami o'n ikki ming besh yuz so'm bo'ldi."),
    ("mehribon", "Voy, rostdan ham! Uzr, bolam. Mana, bir kilo, o'n ming so'm."),
]


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--ref", type=Path, required=True, help="namuna ovoz (WAV)")
    p.add_argument("--chiqish", type=Path, default=Path("natija"))
    p.add_argument("--seed", type=int, default=1986)
    a = p.parse_args()

    navoiy = NAVOIY / "models" / "navoiy-tts"
    sys.path.insert(0, str(NAVOIY / "CosyVoice"))
    sys.path.insert(0, str(NAVOIY / "CosyVoice" / "third_party" / "Matcha-TTS"))
    sys.path.insert(0, str(navoiy))

    import torch
    import torchaudio
    from cosyvoice.cli.cosyvoice import CosyVoice2
    from uztts.normalize import normalize

    uslublar = {e["uz"]: e for e in json.loads((navoiy / "emotions_40h.json").read_text(encoding="utf-8"))}

    t0 = time.perf_counter()
    model = CosyVoice2(str(NAVOIY / "models" / "CosyVoice2-0.5B"), load_jit=False, load_trt=False, fp16=True)
    holat = torch.load(navoiy / "emotion_600h_joint.pt", map_location="cpu", weights_only=True)
    for k in ("state_dict", "model", "llm"):
        if isinstance(holat, dict) and isinstance(holat.get(k), dict):
            holat = holat[k]
    mos = model.model.llm.load_state_dict(holat, strict=False)
    model.model.llm.eval()
    print(f"yuklash: {time.perf_counter() - t0:.1f} s, mos kelmagan kalitlar: {len(mos.missing_keys)}/{len(mos.unexpected_keys)}")

    a.chiqish.mkdir(parents=True, exist_ok=True)
    print(f"{'#':>2} {'uslub':10} {'audio s':>8} {'yozish s':>9} {'RTF':>5}  matn")
    for i, (uslub, matn) in enumerate(GAPLAR, 1):
        torch.manual_seed(a.seed)
        torch.cuda.manual_seed_all(a.seed)
        t = time.perf_counter()
        bolaklar = []
        with torch.inference_mode():
            for r in model.inference_instruct2(normalize(matn, mode="infer"), uslublar[uslub]["instruct"].strip() + "<|endofprompt|>",
                                               str(a.ref.resolve()), stream=False):
                bolaklar.append(r["tts_speech"].detach().cpu())
        torch.cuda.synchronize()
        ketdi = time.perf_counter() - t
        audio = torch.cat(bolaklar, dim=1)
        uzunlik = audio.shape[-1] / 24000
        torchaudio.save(str(a.chiqish / f"{i:02d}_{uslub.replace(chr(39), '')}.wav"), audio, 24000)
        print(f"{i:>2} {uslub:10} {uzunlik:8.2f} {ketdi:9.2f} {ketdi / uzunlik:5.2f}  {matn}")
    print(f"VRAM eng ko'p: {torch.cuda.max_memory_allocated() / 2**30:.2f} GB")


if __name__ == "__main__":
    main()
