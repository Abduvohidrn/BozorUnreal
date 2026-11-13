#!/usr/bin/env python3
"""Navoiy TTS lokal serveri - sotuvchi gapini jonli ovozlashtiradi (UBozorNavoiy ishga tushiradi).

Model: aisha-org/navoiy-tts (Apache-2.0) + FunAudioLLM/CosyVoice2-0.5B (Apache-2.0).
Muhit: muhit/ (D:\\Trk\\Ovoz_tashqi\\navoiy ga junction): .venv, CosyVoice, navoiy-tts.

  GET  /health  -> 200 "ok" (modellar yuklangach), 503 - hali yuklanmoqda
  POST /ayt     {"fel": "mehribon", "ayol": true, "matn": "..."} -> 200 audio/wav (PCM16, 24 kHz, mono)

Ovoz izlari (fe'l x jins x kayfiyat) birinchi so'rovda hisoblanib xotirada qoladi; bir xil gap qayta so'ralsa keshdan.
--ota <pid>: o'yin jarayoni o'lsa server ham o'zini to'xtatadi (o'yin qulab tushsa GPU xotirasi band qolmasin).
"""

from __future__ import annotations

import argparse
import collections
import io
import json
import os
import sys
import threading
import time
import wave
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

PAPKA = Path(__file__).resolve().parent
MUHIT = PAPKA / "muhit"
COSY = MUHIT / "CosyVoice"
BAZA = COSY / "pretrained_models" / "CosyVoice2-0.5B"
NAVOIY = MUHIT / "navoiy-tts"
SR = 24000


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)


class Miya:
    def __init__(self):
        sys.path.insert(0, str(NAVOIY))
        sys.path.insert(0, str(COSY))
        sys.path.insert(0, str(COSY / "third_party" / "Matcha-TTS"))
        # wetext (xitoycha/inglizcha normalizator) har ishga tushishda modelscope.cn'dan yuklashga urinadi (403, ~100 s
        # qayta urinish) va o'zbekchaga kerak emas: o'zbekcha matnni uztts.normalize tayyorlaydi. O'yin internetsiz ishlasin
        sys.modules["wetext"] = None
        sys.modules["ttsfrd"] = None
        os.environ["HF_HUB_OFFLINE"] = "1"
        os.environ["TRANSFORMERS_OFFLINE"] = "1"
        import torch
        from cosyvoice.cli.cosyvoice import CosyVoice2
        from uztts.normalize import normalize

        if not torch.cuda.is_available():
            raise SystemExit("CUDA GPU topilmadi")
        self.torch = torch
        self.normalize = normalize
        t0 = time.time()
        self.model = CosyVoice2(str(BAZA), load_jit=False, load_trt=False, fp16=True)
        state = torch.load(NAVOIY / "emotion_600h_joint.pt", map_location="cpu", weights_only=True)
        for k in ("state_dict", "model", "llm"):
            if isinstance(state, dict) and isinstance(state.get(k), dict):
                state = state[k]
        self.model.model.llm.load_state_dict(state, strict=False)
        # CosyVoice fp16=True faqat autocast qiladi, og'irliklar fp32 qoladi (~2 GB). O'yin, LLM va Navoiy 12 GB videoxotiraga
        # sig'ishi uchun og'irliklar ham fp16: ~1 GB tejaladi. NAVOIY_FP32=1 - eski holat
        if os.environ.get("NAVOIY_FP32") != "1":
            self.model.model.llm.half()
            self.model.model.flow.half()
        self.model.model.llm.eval()
        del state
        torch.cuda.empty_cache()
        emos = json.loads((NAVOIY / "emotions_40h.json").read_text(encoding="utf-8"))
        self.kayfiyat = {e["uz"]: e["instruct"].strip() + "<|endofprompt|>" for e in emos}
        self.fellar = json.loads((PAPKA / "ovozlar.json").read_text(encoding="utf-8"))
        self.izlar: set[str] = set()
        self.kesh: collections.OrderedDict[str, bytes] = collections.OrderedDict()
        self.qulf = threading.Lock()
        log(f"modellar yuklandi {time.time() - t0:.1f} s")

    def iz(self, fel: str, ayol: bool) -> tuple[str, float]:
        f = self.fellar.get(fel) or self.fellar["mehribon"]
        kayf = f.get("kayfiyat", "xotirjam")
        ovoz = f"{fel if fel in self.fellar else 'mehribon'}_{'ayol' if ayol else 'erkak'}"
        kalit = f"{ovoz}|{kayf}"
        if kalit not in self.izlar:
            wav = PAPKA / "ovozlar" / f"{ovoz}.wav"
            # instruct2: prompt_text o'rnida kayfiyat ko'rsatmasi turadi - shuning uchun iz (ovoz, kayfiyat) juftiga
            self.model.add_zero_shot_spk(self.kayfiyat[kayf], str(wav), kalit)
            self.izlar.add(kalit)
            log(f"ovoz izi: {kalit}")
        return kalit, float(f.get("tezlik", 1.0))

    def ayt(self, fel: str, ayol: bool, matn: str) -> bytes:
        with self.qulf:
            kalit, tezlik = self.iz(fel, ayol)
            k = f"{kalit}|{matn}"
            if k in self.kesh:
                self.kesh.move_to_end(k)
                return self.kesh[k]
            t0 = time.time()
            text = self.normalize(matn, mode="infer")
            qism = []
            with self.torch.inference_mode():
                for r in self.model.inference_instruct2(text, self.kayfiyat[self.fellar.get(fel, self.fellar["mehribon"])["kayfiyat"]],
                                                         "", zero_shot_spk_id=kalit, stream=False, speed=tezlik):
                    qism.append(r["tts_speech"].detach().float().cpu())
            if not qism:
                raise RuntimeError("model ovoz qaytarmadi")
            audio = self.torch.cat(qism, dim=1).squeeze(0).clamp(-1, 1)
            pcm = (audio * 32767).short().numpy().tobytes()
            buf = io.BytesIO()
            with wave.open(buf, "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(SR)
                w.writeframes(pcm)
            data = buf.getvalue()
            uz = len(pcm) / 2 / SR
            log(f"{kalit} {uz:.1f} s ovoz, {time.time() - t0:.1f} s: {matn}")
            self.kesh[k] = data
            while len(self.kesh) > 200:
                self.kesh.popitem(last=False)
            return data


MIYA: Miya | None = None


class Ishlov(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _javob(self, kod: int, tana: bytes, tur: str = "text/plain; charset=utf-8"):
        self.send_response(kod)
        self.send_header("Content-Type", tur)
        self.send_header("Content-Length", str(len(tana)))
        self.end_headers()
        self.wfile.write(tana)

    def do_GET(self):
        if self.path == "/health":
            self._javob(200 if MIYA else 503, b"ok" if MIYA else b"yuklanmoqda")
        else:
            self._javob(404, b"yo'q")

    def do_POST(self):
        if self.path != "/ayt":
            return self._javob(404, b"yo'q")
        if not MIYA:
            return self._javob(503, b"yuklanmoqda")
        try:
            n = int(self.headers.get("Content-Length", "0"))
            j = json.loads(self.rfile.read(n).decode("utf-8"))
            matn = str(j.get("matn", "")).strip()
            if not matn:
                return self._javob(400, b"matn bo'sh")
            data = MIYA.ayt(str(j.get("fel", "mehribon")), bool(j.get("ayol", False)), matn)
            self._javob(200, data, "audio/wav")
        except Exception as e:  # o'yin tayyor ovozga qaytadi
            log("xato:", repr(e))
            self._javob(500, repr(e).encode("utf-8"))


def ota_kuzat(pid: int):
    """Windows: ota jarayon tugashini kutadi va serverni to'xtatadi."""
    import ctypes
    k32 = ctypes.windll.kernel32
    h = k32.OpenProcess(0x00100000, False, pid)  # SYNCHRONIZE
    if not h:
        log(f"ota jarayon {pid} topilmadi - chiqaman")
        os._exit(0)
    k32.WaitForSingleObject(h, 0xFFFFFFFF)
    log("ota jarayon tugadi - chiqaman")
    os._exit(0)


def main():
    global MIYA
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=18600)
    p.add_argument("--ota", type=int, default=0)
    p.add_argument("--log", default="")
    a = p.parse_args()
    if a.log:
        Path(a.log).parent.mkdir(parents=True, exist_ok=True)
        f = open(a.log, "w", encoding="utf-8", buffering=1)
        sys.stdout = sys.stderr = f
    if a.ota:
        threading.Thread(target=ota_kuzat, args=(a.ota,), daemon=True).start()
    srv = ThreadingHTTPServer(("127.0.0.1", a.port), Ishlov)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    log(f"port {a.port}, modellar yuklanmoqda...")
    m = Miya()
    # birinchi so'rov sekin bo'lmasin: CUDA yadrolari va eng ko'p ishlatiladigan ovoz izi oldindan
    m.ayt("mehribon", True, "Keling.")
    for fel in m.fellar:
        if fel.startswith("_"):
            continue
        for ayol in (True, False):
            with m.qulf:
                m.iz(fel, ayol)
    MIYA = m
    log("tayyor")
    threading.Event().wait()


if __name__ == "__main__":
    main()
