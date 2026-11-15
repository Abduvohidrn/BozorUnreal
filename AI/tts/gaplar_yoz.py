#!/usr/bin/env python3
"""Sotuvchi gaplarini Navoiy bilan bo'laklab oldindan yozadi -> natija: gaplar/<nom>.wav (Scripts/import_gaplar.py import qiladi).

Bo'laklar o'yin bilan bir xil (UBozorTayyorJavob::Yoz, BozorSotuv.cpp):
  qolip matni o'rinlar ({narx} ...) orasida  -> G_<md5(utf8)[:8]>_<ovoz>_<uslub>  (har fe'l uslubida)
  son: minglar va qoldiq (12 500)            -> S_12m_<ovoz>, S_500_<ovoz>
  miqdor (o'ndan bir: 2,5)                   -> M_25_<ovoz>
  mol / birlik                               -> P_<id>_<ovoz>, B_<birlik tutuqsiz>_<ovoz>
Ovozlar: ayol / erkak - CosyVoice repo namunalari (Apache-2.0). Bor fayllar qayta yozilmaydi (to'xtab qolsa davom etadi).
Sifat nazorati: har bo'lak o'yin STT'si (AI/stt/crispasr.exe, WSL'dan Windows dasturi sifatida) bilan matnga qaytariladi;
asl matnga o'xshashligi MOS dan past bo'lsa boshqa seed bilan qayta yoziladi (URINISH martagacha), eng o'xshashi qoladi.
Natija: gaplar/sifat.tsv (nom, o'xshashlik, eshitilgani). --tekshir - bor fayllarni ham tekshiradi.

WSL'da: ~/navoiy/.venv/bin/python gaplar_yoz.py [--ovoz ayol] [--faqat-royxat] [--tekshir]
"""

from __future__ import annotations

import argparse
import difflib
import hashlib
import json
import re
import subprocess
import sys
import time
from pathlib import Path

NAVOIY = Path.home() / "navoiy"
BU = Path(__file__).resolve().parent
SOTUV = BU.parent.parent / "Content" / "Bozor" / "Data" / "sotuv.json"
CHIQISH = BU / "gaplar"
NAMUNA = {
    "ayol": NAVOIY / "CosyVoice" / "asset" / "zero_shot_prompt.wav",
    "erkak": NAVOIY / "CosyVoice" / "asset" / "cross_lingual_prompt.wav",
}
SONLAR_MING = 300   # 1 000 ... 300 000 so'mgacha
STT = BU.parent / "stt"
MOS = 0.9
URINISH = 5
SEEDLAR = [1986, 7, 42, 2026, 99]


def winyol(p: Path) -> str:
    return subprocess.run(["wslpath", "-w", str(p)], capture_output=True, text=True).stdout.strip()


def toza(s: str) -> str:
    s = re.sub("[ʻʼ‘’`]", "'", s.lower())
    return re.sub(r"\s+", " ", re.sub(r"[^\w' ]", " ", s)).strip()


def eshit(wav: Path) -> str:
    """O'yin STT'si bilan matnga qaytaradi (BozorQuloq.cpp dagi argumentlar)."""
    of = BU / "natija" / "stt" / wav.stem
    of.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(STT / "crispasr.exe"), "--backend", "fastconformer-ctc", "-m", winyol(STT / "stt-uz.gguf"), "-f", winyol(wav),
                    "-t", "4", "-nt", "-ng", "-np", "--lid-backend", "off", "--no-punctuation", "-otxt", "-of", winyol(of)],
                   capture_output=True)
    txt = of.with_suffix(".txt")
    return txt.read_text(encoding="utf-8").strip() if txt.exists() else ""


def oxshash(aytilgan: str, eshitildi: str) -> float:
    return difflib.SequenceMatcher(None, toza(aytilgan), toza(eshitildi)).ratio()

BIRLAR = ["", "bir", "ikki", "uch", "to'rt", "besh", "olti", "yetti", "sakkiz", "to'qqiz"]
ONLAR = ["", "o'n", "yigirma", "o'ttiz", "qirq", "ellik", "oltmish", "yetmish", "sakson", "to'qson"]


def mingacha(n: int) -> str:
    """1..999 -> so'z bilan ("yuz", "ikki yuz o'n besh")."""
    y, q = divmod(n, 100)
    s = [] if not y else ["yuz" if y == 1 else BIRLAR[y] + " yuz"]
    if q // 10:
        s.append(ONLAR[q // 10])
    if q % 10:
        s.append(BIRLAR[q % 10])
    return " ".join(s)


def miqdor_soz(o: int) -> str:
    """o'ndan bir (25 -> 2,5) -> "ikki yarim"."""
    b, q = divmod(o, 10)
    s = mingacha(b) if b else ""
    if q == 5:
        s = (s + " yarim").strip()
    return s


def matn_kaliti(bolak: str) -> str:
    return "G_" + hashlib.md5(bolak.encode("utf-8")).hexdigest()[:8]


def qolip_bolaklari(q: str) -> list[str]:
    """Qolipning matn bo'laklari (o'rinlar orasidagi, harfi borlari) - C++ dagi kabi."""
    return [t.strip() for t in re.split(r"\{[^}]*\}", q) if any(c.isalpha() for c in t.strip())]


def royxat() -> list[tuple[str, str, str | None]]:
    """(kalit, aytiladigan matn, uslub yoki None) - uslubsizlari bitta neytral ovozda."""
    d = json.loads(SOTUV.read_text(encoding="utf-8"))
    uslublar = sorted({f.get("uslub", "xotirjam") for f in d["fellar"].values()})
    out: dict[tuple[str, str | None], str] = {}

    def matn(bolak: str, uslub: str | None):
        out[(matn_kaliti(bolak), uslub)] = bolak

    for variantlar in d["gaplar"].values():
        for q in variantlar:
            for b in qolip_bolaklari(q):
                for u in uslublar:
                    matn(b, u)
    for f in d["fellar"].values():
        for variantlar in f.get("gaplar", {}).values():
            for q in variantlar:
                for b in qolip_bolaklari(q):
                    matn(b, f.get("uslub", "xotirjam"))
    for b in ("va", "so'm"):   # ro'yxat va qaytim orasidagi bo'laklar
        for u in uslublar:
            matn(b, u)

    for a in range(1, SONLAR_MING + 1):
        out[(f"S_{a}m", None)] = "ming" if a == 1 else mingacha(a) + " ming"
    out[("S_500", None)] = "besh yuz"
    out[("S_0", None)] = "nol"
    for o in range(5, 201, 5):
        out[(f"M_{o}", None)] = miqdor_soz(o)
    for m in d["mahsulotlar"]:
        out[(f"P_{m['id']}", None)] = m["nomi"]
    for b in {m["birlik"] for m in d["mahsulotlar"]}:
        out[("B_" + b.replace("'", ""), None)] = b
    return [(k, t, u) for (k, u), t in out.items()]


def nomi(kalit: str, ovoz: str, uslub: str | None) -> str:
    return f"{kalit}_{ovoz}" + (f"_{uslub.replace(chr(39), '')}" if uslub else "")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--ovoz", choices=sorted(NAMUNA), action="append", help="faqat shu ovoz(lar)")
    p.add_argument("--faqat-royxat", action="store_true", help="yozmasdan, nechta bo'lak borligini ko'rsatadi")
    p.add_argument("--tekshir", action="store_true", help="bor fayllarni ham STT bilan tekshirib, yomonlarini qayta yozadi")
    a = p.parse_args()
    ovozlar = a.ovoz or sorted(NAMUNA)

    R = royxat()
    ishlar = [(nomi(k, o, u), t, u) for o in ovozlar for k, t, u in R]
    CHIQISH.mkdir(parents=True, exist_ok=True)
    (CHIQISH / "royxat.tsv").write_text("".join(f"{n}\t{t}\n" for n, t, _ in ishlar), encoding="utf-8")
    sifat_fayl = CHIQISH / "sifat.tsv"
    sifat = {}   # nom -> (o'xshashlik, eshitildi): tekshirilganlari qayta tekshirilmaydi
    if sifat_fayl.exists():
        for q in sifat_fayl.read_text(encoding="utf-8").splitlines():
            n, o, e = (q.split("\t") + ["", ""])[:3]
            sifat[n] = (float(o or 0), e)
    qoldi = [x for x in ishlar if not (CHIQISH / f"{x[0]}.wav").exists() or (a.tekshir and x[0] not in sifat)]
    print(f"bo'laklar: {len(R)} x {len(ovozlar)} ovoz = {len(ishlar)}, yoziladi/tekshiriladi: {len(qoldi)}")
    if a.faqat_royxat or not qoldi:
        return

    navoiy = NAVOIY / "models" / "navoiy-tts"
    sys.path.insert(0, str(NAVOIY / "CosyVoice"))
    sys.path.insert(0, str(NAVOIY / "CosyVoice" / "third_party" / "Matcha-TTS"))
    sys.path.insert(0, str(navoiy))
    import torch
    import torchaudio
    from cosyvoice.cli.cosyvoice import CosyVoice2
    from uztts.normalize import normalize

    yoriq = {e["uz"]: e["instruct"].strip() + "<|endofprompt|>" for e in json.loads((navoiy / "emotions_40h.json").read_text(encoding="utf-8"))}
    model = CosyVoice2(str(NAVOIY / "models" / "CosyVoice2-0.5B"), load_jit=False, load_trt=False, fp16=True)
    holat = torch.load(navoiy / "emotion_600h_joint.pt", map_location="cpu", weights_only=True)
    for k in ("state_dict", "model", "llm"):
        if isinstance(holat, dict) and isinstance(holat.get(k), dict):
            holat = holat[k]
    model.model.llm.load_state_dict(holat, strict=False)
    model.model.llm.eval()

    SR = 24000

    def yoz(fayl: Path, matn: str, uslub: str | None, ovoz: str, seed: int):
        aytish = re.sub(r"^[\s.,:;!?\-–—]+", "", matn)   # ". Jami" -> "Jami", "- bir" -> "bir"
        torch.manual_seed(seed)
        torch.cuda.manual_seed_all(seed)
        bolaklar = []
        with torch.inference_mode():
            for r in model.inference_instruct2(normalize(aytish, mode="infer"), yoriq[uslub or "xotirjam"], str(NAMUNA[ovoz]),
                                               stream=False, speed=1.1 if uslub == "qo'rqqan" else 1.0):
                bolaklar.append(r["tts_speech"].detach().cpu())
        audio = torch.cat(bolaklar, dim=1)[0]
        # jimlikni kesish (10 ms qoldirib), balandlikni tenglash (RMS -20 dBFS, cho'qqi 0.95 dan oshmaydi)
        tovush = (audio.abs() > 0.02 * audio.abs().max()).nonzero()
        if len(tovush):
            audio = audio[max(0, int(tovush[0]) - SR // 100): int(tovush[-1]) + SR // 100]
        rms = audio.pow(2).mean().sqrt().clamp(min=1e-6)
        audio = audio * min(0.1 / rms, 0.95 / audio.abs().max().clamp(min=1e-6))
        # bo'lakdan keyingi to'xtam: gap oxiri 250 ms, vergul 120 ms, so'z orasi 40 ms
        tox = 0.25 if re.search(r"[.!?]$", matn) else 0.12 if re.search(r"[,:;\-–—]$", matn) else 0.04
        audio = torch.cat([audio, torch.zeros(int(SR * tox))])
        torchaudio.save(str(fayl), audio.unsqueeze(0), SR, bits_per_sample=16)

    t0 = time.perf_counter()
    qayta = 0
    with sifat_fayl.open("a", encoding="utf-8") as sf:
        for i, (n, matn, uslub) in enumerate(qoldi, 1):
            ovoz = n.split("_")[2] if n.startswith("G_") else n.rsplit("_", 1)[1]
            fayl = CHIQISH / f"{n}.wav"
            if not fayl.exists():
                yoz(fayl, matn, uslub, ovoz, SEEDLAR[0])
            eng = (oxshash(matn, e := eshit(fayl)), e)
            # yomon chiqsa - boshqa seed'lar; eng o'xshashi fayl bo'lib qoladi
            for seed in SEEDLAR[1:URINISH]:
                if eng[0] >= MOS:
                    break
                qayta += 1
                sinov = CHIQISH / f"{n}.sinov.wav"
                yoz(sinov, matn, uslub, ovoz, seed)
                o = (oxshash(matn, e := eshit(sinov)), e)
                if o[0] > eng[0]:
                    sinov.replace(fayl)
                    eng = o
                else:
                    sinov.unlink()
            sf.write(f"{n}\t{eng[0]:.2f}\t{eng[1]}\n")
            sf.flush()
            if i % 25 == 0 or i == len(qoldi):
                o = (time.perf_counter() - t0) / i
                print(f"{i}/{len(qoldi)}  {o:.2f} s/bo'lak, qayta yozildi {qayta}, qoldi ~{o * (len(qoldi) - i) / 60:.0f} daq"
                      f"  ({n}: {matn} -> {eng[1]} {eng[0]:.2f})", flush=True)


if __name__ == "__main__":
    main()
