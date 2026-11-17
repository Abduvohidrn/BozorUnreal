#!/usr/bin/env python3
"""Sotuvchi LLM sinovi: Ollama modellari bozor holatlarida o'zbekcha qanday javob yozadi.

Har holatda modelga bola gapi, VAZIFA (javob turi), MAJBURIY sonlar va NAMUNA gap beriladi; model sotuvchining
1-2 qisqa gapini yozadi. Narx/pulni o'yin kodi hisoblaydi - model faqat aytadi, shuning uchun o'lchanadi:
  sonlar   - MAJBURIY sonlarning hammasi aytildimi, o'zidan boshqa son qo'shdimi
  til      - kirill/ingliz harflari yo'qmi, 25 so'zdan oshmadimi
  vaqt     - javob vaqti (model yuklangandan keyin)
Faqat standart kutubxona. Ishga tushirish: uv run --no-project --python 3.14.0 AI/miya/llm_sinov.py [model ...]
Natija: AI/miya/natija/llm_<model>.json va ekranda jadval.
"""

from __future__ import annotations

import json
import re
import sys
import time
import urllib.request
from pathlib import Path

OLLAMA = "http://127.0.0.1:11434/api/chat"
BU = Path(__file__).resolve().parent

FEL = {
    "mehribon": "mehribon va iliq, bolaga mehr bilan gapirasan",
    "qattiq": "qattiqqo'l, qisqa va lo'nda gapirasan, ortiqcha gapni yoqtirmaysan",
    "shoshqaloq": "shoshqaloq, tez-tez gapirasan, savdoni tezroq tugatmoqchisan",
    "ayyor": "ayyor va muloyim, molingni maqtaysan, biroz hazilkashsan",
}

SYSTEM = """Sen Toshkentdagi dehqon bozorida savdo qiladigan sotuvchisan. Fe'ling: {fel}.
Xaridor - 10-13 yoshli bola, unga "bolam" deb murojaat qilasan.

Qoidalar:
- Faqat o'zbek tilida, lotin yozuvida gapir.
- 1-2 qisqa gap, jami 20 so'zdan oshmasin.
- Har safar VAZIFA beriladi - aynan shuni ayt.
- MAJBURIY sonlarni aynan berilgan raqamlarda yoz (masalan "12 000 so'm"). O'zingdan boshqa narx, son yoki chegirma o'ylab topma.
- NAMUNA - faqat ma'no uchun, so'zma-so'z takrorlama, o'z fe'lingga moslab ayt.
- Faqat sotuvchining gapini yoz: izoh, qo'shtirnoq, emoji, ism yozma."""

# few-shot: "ko'rsat, aytma" - format va sonlarni aynan aytishni misol bilan
MISOLLAR = [
    ("Bola: \"Assalomu alaykum, olma necha pul?\"\nVAZIFA: olmaning narxini ayt\nMAJBURIY: 14 000 so'm (1 kilo)\n"
     "NAMUNA: Bir kilo olma 14 000 so'm.",
     "Va alaykum assalom, bolam! Olmaning kilosi 14 000 so'm, shirin-shirin."),
    ("Bola: \"Iltimos, arzonroq qilib bering\"\nVAZIFA: narxni tushirganingni ayt\nMAJBURIY: 9 500 so'm\n"
     "NAMUNA: Mayli, siz uchun 9 500 so'm qilaman.",
     "Mayli bolam, odobingiz uchun 9 500 so'mdan beraman."),
    ("Bola: \"Mana, 20 000 so'm\"\nVAZIFA: pulni olib, qaytimni ber\nMAJBURIY: qaytim 5 000 va 2 000 so'm\n"
     "NAMUNA: Rahmat! Mana qaytimingiz: 5 000 va 2 000 so'm.",
     "Rahmat, bolam! Mana qaytimingiz: 5 000 va 2 000 so'm, sanab oling."),
]

# (fe'l, bola gapi, vazifa, majburiy sonlar, namuna)
HOLATLAR = [
    ("mehribon", "Assalomu alaykum!", "salomga alik ol, nima olishini so'ra; mollaring: pomidor, bodring", [],
     "Va alaykum assalom, bolam! Nima olasiz? Pomidor, bodring bor."),
    ("qattiq", "Pomidor necha pul?", "pomidorning narxini ayt", ["12 000"], "Bir kilo pomidor 12 000 so'm."),
    ("mehribon", "Iltimos, ozgina arzonroq qilib bering", "narxni tushirganingni ayt", ["11 000"],
     "Mayli, siz uchun 11 000 so'm qilaman."),
    ("qattiq", "Qimmat-ku! Bunaqa narx bo'lmaydi!", "bola qo'pol gapirdi - narxni tushirmasligingni ayt", ["12 000"],
     "Kattalarga odob bilan gapirish kerak. Narxi 12 000 so'm."),
    ("ayyor", "Yana arzonroq bo'lmaydimi?", "bundan arzon bermasligingni ayt", ["10 000"],
     "Bundan arzoni yo'q, 10 000 so'm, o'zimga ham shunga tushgan."),
    ("shoshqaloq", "Ikki yarim kilo bering", "tortib, jami summani ayt", ["2,5", "12 500"],
     "Mana, 2,5 kilo kartoshka. Jami 12 500 so'm bo'ldi."),
    ("mehribon", "Kechirasiz, men 2 kilo so'ragandim", "ko'p tortganingni tan olib uzr so'ra, to'g'rila", ["2", "10 000"],
     "Voy, uzr bolam. Mana, 2 kilo, 10 000 so'm."),
    ("ayyor", "Mana, 50 000 so'm", "pulni olib, qaytimni ber", ["37 500"],
     "Rahmat! Mana qaytimingiz: 37 500 so'm."),
    ("shoshqaloq", "Kechirasiz, qaytimni kam berdingiz", "kam qaytim berganingni tan olib, qolganini ber", ["2 000"],
     "Voy, kechirasiz, adashibman! Mana yana 2 000 so'm."),
    ("qattiq", "Hoy, tez bo'ling!", "bola juda qo'pol - savdoni to'xtat, boshqa rastaga yubor", [],
     "Bunaqa gaplashadigan bo'lsangiz, boshqa joydan oling!"),
    ("mehribon", "Amaki, bu pomidor qayerdan keltirilgan?", "savolga qisqa javob ber (o'zing o'ylab topishing mumkin), so'ng olasizmi deb so'ra; narx aytma", [],
     "Bu pomidor Toshkent viloyatidan, bolam. Olasizmi?"),
    ("mehribon", "Rahmat, xayr!", "xayrlash", [], "Yana keling, bolam! Ota-onangizga salom ayting!"),
]


def sonlar(s: str) -> list[str]:
    """Matndagi sonlar: "12 000" va "12000" bir xil, "2,5" kasr."""
    return [re.sub(r"[  .]", "", x) for x in re.findall(r"\d+(?:[  .]\d{3})*(?:,\d+)?", s)]


def chat(model: str, xabarlar: list[dict]) -> tuple[str, float]:
    sorov = {"model": model, "messages": xabarlar, "stream": False, "think": False, "keep_alive": "10m",
             "options": {"temperature": 0.6, "num_predict": 90}}
    t = time.perf_counter()
    req = urllib.request.Request(OLLAMA, json.dumps(sorov).encode("utf-8"), {"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=180) as r:
        javob = json.load(r)
    return javob["message"]["content"].strip(), time.perf_counter() - t


def sina(model: str) -> list[dict]:
    chat(model, [{"role": "user", "content": "Salom"}])   # modelni xotiraga yuklash (vaqtga kirmaydi)
    natija = []
    for fel, gap, vazifa, majburiy, namuna in HOLATLAR:
        x = [{"role": "system", "content": SYSTEM.format(fel=FEL[fel])}]
        for s, j in MISOLLAR:
            x += [{"role": "user", "content": s}, {"role": "assistant", "content": j}]
        x.append({"role": "user", "content": f"Bola: \"{gap}\"\nVAZIFA: {vazifa}\nMAJBURIY: "
                                            f"{', '.join(majburiy) + ' so‘m' if majburiy else 'yo‘q'}\nNAMUNA: {namuna}"})
        javob, vaqt = chat(model, x)
        bor = sonlar(javob)
        kerak = [re.sub(r"[ .]", "", m) for m in majburiy]
        yoq = [m for m in kerak if m not in bor]
        ortiqcha = [b for b in bor if b not in kerak]
        til_xato = bool(re.search(r"[Ѐ-ӿ]", javob)) or len(javob.split()) > 25
        natija.append({"fel": fel, "bola": gap, "javob": javob, "vaqt": round(vaqt, 2),
                       "son_yoq": yoq, "son_ortiqcha": ortiqcha, "til_xato": til_xato})
    return natija


def main() -> None:
    modellar = sys.argv[1:] or ["gemma4:12b", "qwen2.5:7b-instruct"]
    (BU / "natija").mkdir(exist_ok=True)
    for m in modellar:
        r = sina(m)
        (BU / "natija" / f"llm_{m.replace(':', '_')}.json").write_text(json.dumps(r, ensure_ascii=False, indent=1), encoding="utf-8")
        tog = sum(1 for x in r if not x["son_yoq"] and not x["son_ortiqcha"] and not x["til_xato"])
        vaqt = sorted(x["vaqt"] for x in r)
        print(f"\n===== {m}: to'g'ri {tog}/{len(r)}, vaqt o'rtacha {sum(vaqt) / len(vaqt):.2f} s, eng uzun {vaqt[-1]:.2f} s")
        for x in r:
            belgi = "OK" if not x["son_yoq"] and not x["son_ortiqcha"] and not x["til_xato"] else "--"
            izoh = (f" [yo'q: {x['son_yoq']}]" if x["son_yoq"] else "") + (f" [ortiqcha: {x['son_ortiqcha']}]" if x["son_ortiqcha"] else "") \
                + (" [til]" if x["til_xato"] else "")
            print(f"{belgi} {x['vaqt']:5.2f}s {x['fel']:10} {x['bola'][:28]:28} -> {x['javob']}{izoh}")


if __name__ == "__main__":
    main()
