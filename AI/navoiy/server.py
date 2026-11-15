#!/usr/bin/env python3
"""Navoiy TTS lokal serveri - sotuvchi gapini jonli ovozlashtiradi (UBozorNavoiy ishga tushiradi).

Model: aisha-org/navoiy-tts (Apache-2.0) + FunAudioLLM/CosyVoice2-0.5B (Apache-2.0).
Muhit: muhit/ (Windows: .venv, CosyVoice, navoiy-tts) yoki --muhit <papka> (WSL: ~/navoiy - .venv, CosyVoice,
models/CosyVoice2-0.5B, models/navoiy-tts). AI/navoiy/wsl.txt bo'lsa UBozorNavoiy serverni WSL ichida ishga tushiradi.

  GET  /health  -> 200 "ok" (modellar yuklangach), 503 - hali yuklanmoqda
  POST /ayt     {"fel": "mehribon", "ayol": true, "matn": "..."} -> 200 audio/wav (PCM16, 24 kHz, mono)
  POST /ayt_oqim  (xuddi shu tana) -> 200 "PCM1" + chastota (uint32 LE) + PCM16 mono bo'laklari, tayyor bo'lishi bilan

Ovoz izlari (fe'l x jins x kayfiyat) birinchi so'rovda hisoblanib xotirada qoladi; bir xil gap qayta so'ralsa keshdan.
--ota <pid>: o'yin jarayoni o'lsa server ham o'zini to'xtatadi (o'yin qulab tushsa GPU xotirasi band qolmasin).
--stdin: stdin yopilsa (o'yin wsl.exe ni to'xtatdi yoki quvurni yopdi) server chiqadi.
"""

from __future__ import annotations

import argparse
import collections
import io
import json
import os
import struct
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


def muhit_ornat(muhit: Path):
    """Model yo'llari: Windows muhiti (CosyVoice/pretrained_models, navoiy-tts) yoki WSL (~/navoiy/models/...)."""
    global MUHIT, COSY, BAZA, NAVOIY
    MUHIT = muhit
    COSY = muhit / "CosyVoice"
    BAZA = next((p for p in (COSY / "pretrained_models" / "CosyVoice2-0.5B", muhit / "models" / "CosyVoice2-0.5B") if p.is_dir()),
                COSY / "pretrained_models" / "CosyVoice2-0.5B")
    NAVOIY = next((p for p in (muhit / "navoiy-tts", muhit / "models" / "navoiy-tts") if p.is_dir()), muhit / "navoiy-tts")


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
        # o'yin so'rovlari soni (navbatda yoki bajarilayotgan): fondagi ovoz izlarini tayyorlash shular tugashini kutadi
        self.sorovlar = 0
        log(f"modellar yuklandi {time.time() - t0:.1f} s")

    def iz(self, fel: str, ayol: bool, ohang: float = 0.0, namuna: str = "") -> tuple[str, float]:
        """Ovoz izi (fe'l x jins x kayfiyat x ohang). ohang - namuna ovozni necha yarim tonga ko'tarish/tushirish (0.5 qadam):
        har sotuvchiga o'z ovozi (xulq.json "ovoz.ohang"). Navoiy shu o'zgargan namunadan nusxa oladi - tembr va balandlik
        boshqacha, gap tezligi esa tabiiy qoladi."""
        f = self.fellar.get(fel) or self.fellar["mehribon"]
        kayf = f.get("kayfiyat", "xotirjam")
        ovoz = f"{fel if fel in self.fellar else 'mehribon'}_{'ayol' if ayol else 'erkak'}"
        # namuna: sotuvchining o'z ovozi (ovozlar/<namuna>.wav, masalan "odamlar/erkak_cv_137014" - Common Voice, CC0).
        # Bo'lmasa fe'l x jins namunasi (hammasi ikki asl ovozdan - sotuvchilar bir-biriga o'xshab qolardi)
        namuna = "".join(c for c in namuna if c.isalnum() or c in "_/-")
        if namuna and (PAPKA / "ovozlar" / f"{namuna}.wav").exists():
            ovoz = namuna
        ohang = max(-6.0, min(6.0, round(ohang * 2) / 2))
        kalit = f"{ovoz}|{kayf}" + (f"|{ohang:+.1f}" if ohang else "")
        if kalit not in self.izlar:
            wav = PAPKA / "ovozlar" / f"{ovoz}.wav"
            if ohang:
                wav = self.ohangli(wav, ohang)
            # instruct2: prompt_text o'rnida kayfiyat ko'rsatmasi turadi - shuning uchun iz (ovoz, kayfiyat) juftiga
            self.model.add_zero_shot_spk(self.kayfiyat[kayf], str(wav), kalit)
            self.izlar.add(kalit)
            log(f"ovoz izi: {kalit}")
        return kalit, float(f.get("tezlik", 1.0))

    def ohangli(self, wav: Path, ohang: float) -> Path:
        """Namuna ovozni ohang yarim tonga siljitadi (tezligi o'zgarmaydi) va vaqtinchalik faylga yozadi."""
        import tempfile
        import torchaudio
        chiq = Path(tempfile.gettempdir()) / "navoiy_ovozlar" / f"{wav.parent.name}_{wav.stem}_{ohang:+.1f}.wav"
        if not chiq.exists():
            chiq.parent.mkdir(parents=True, exist_ok=True)
            w, sr = torchaudio.load(str(wav))
            w = torchaudio.functional.pitch_shift(w, sr, n_steps=int(round(ohang * 2)), bins_per_octave=24)
            torchaudio.save(str(chiq), w.clamp(-1, 1), sr)
        return chiq

    def ayt_oqim(self, fel: str, ayol: bool, matn: str, yoz, ohang: float = 0.0, namuna: str = "") -> bytes:
        """Matnni ovozlashtiradi; har tayyor bo'lak (PCM16 mono SR) darhol yoz(bytes) ga beriladi. To'liq PCM qaytadi.
        Model matnni gaplarga bo'ladi (har gap - bo'lak); tezlik 1 bo'lsa stream=True gap ichini ham ~1-2 s lik bo'laklarga
        bo'ladi (CosyVoice'da tezlik o'zgarishi faqat oqimsiz rejimda). yoz xato bersa (o'yin uzildi) - yozish to'xtaydi."""
        self.sorovlar += 1
        try:
            return self._ayt_oqim(fel, ayol, matn, yoz, ohang, namuna)
        finally:
            self.sorovlar -= 1

    def _ayt_oqim(self, fel: str, ayol: bool, matn: str, yoz, ohang: float, namuna: str = "") -> bytes:
        with self.qulf:
            kalit, tezlik = self.iz(fel, ayol, ohang, namuna)
            k = f"{kalit}|{matn}"
            if k in self.kesh:
                self.kesh.move_to_end(k)
                yoz(self.kesh[k])
                return self.kesh[k]
            t0 = time.time()
            birinchi = 0.0
            text = self.normalize(matn, mode="infer")
            oqim = abs(tezlik - 1.0) < 1e-3 and os.environ.get("NAVOIY_OQIM", "1") != "0"
            qism: list[bytes] = []
            # CosyVoice2Model.tts oqim paytida token_hop_len ni modelning o'zida oshirib qo'yadi (25 -> 50 -> 100) va qaytarmaydi:
            # birinchi oqimli gapdan keyin bo'laklar 4 s lik bo'lib qolardi (qisqa gap - bitta bo'lak). Har gap boshida 25 (o'qitishdagi chunk)
            self.model.model.token_hop_len = 25
            # Balandlik: Navoiy ovozi juda past chiqadi (cho'qqi ~0.15, o'rtacha ~-34 dB) - o'yinda 3D so'nish va bozor shovqini
            # ichida eshitilmay qolardi. Yozilgan bo'laklar (AI/tts/gaplar_yoz.py) bilan bir xil qoida: RMS -20 dBFS (0.1),
            # cho'qqi 0.95 dan oshmaydi. Kuchaytirish birinchi bo'lakdan hisoblanadi va butun gapga bir xil qo'llanadi
            # (oqimda gap o'rtasida balandlik sakramasin); keyingi bo'laklar cho'qqisi kesiladi. NAVOIY_BALAND=0 - o'chirish
            kuch = None if os.environ.get("NAVOIY_BALAND", "1") != "0" else 1.0
            with self.torch.inference_mode():
                for r in self.model.inference_instruct2(text, self.kayfiyat[self.fellar.get(fel, self.fellar["mehribon"])["kayfiyat"]],
                                                         "", zero_shot_spk_id=kalit, stream=oqim, speed=tezlik):
                    audio = r["tts_speech"].detach().float().cpu().squeeze(0)
                    if kuch is None:
                        rms = max(float(audio.pow(2).mean().sqrt()), 1e-5)
                        kuch = min(20.0, max(1.0, min(0.1 / rms, 0.95 / max(float(audio.abs().max()), 1e-4))))
                    audio = (audio * kuch).clamp(-0.98, 0.98)
                    b = (audio * 32767).short().numpy().tobytes()
                    if not birinchi:
                        birinchi = time.time() - t0
                    qism.append(b)
                    yoz(b)
            if not qism:
                raise RuntimeError("model ovoz qaytarmadi")
            pcm = b"".join(qism)
            log(f"{kalit} {len(pcm) / 2 / SR:.1f} s ovoz, {len(qism)} bo'lak, kuch x{kuch or 1.0:.1f}, birinchisi {birinchi:.1f} s, jami {time.time() - t0:.1f} s: {matn}")
            self.kesh[k] = pcm
            while len(self.kesh) > 200:
                self.kesh.popitem(last=False)
            return pcm

    def ayt(self, fel: str, ayol: bool, matn: str, ohang: float = 0.0, namuna: str = "") -> bytes:
        pcm = self.ayt_oqim(fel, ayol, matn, lambda b: None, ohang, namuna)
        buf = io.BytesIO()
        with wave.open(buf, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(SR)
            w.writeframes(pcm)
        return buf.getvalue()


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
        if self.path not in ("/ayt", "/ayt_oqim"):
            return self._javob(404, b"yo'q")
        if not MIYA:
            return self._javob(503, b"yuklanmoqda")
        boshlandi = False
        try:
            n = int(self.headers.get("Content-Length", "0"))
            j = json.loads(self.rfile.read(n).decode("utf-8"))
            matn = str(j.get("matn", "")).strip()
            if not matn:
                return self._javob(400, b"matn bo'sh")
            fel, ayol = str(j.get("fel", "mehribon")), bool(j.get("ayol", False))
            ohang = float(j.get("ohang", 0.0) or 0.0)
            namuna = str(j.get("namuna", "") or "")
            if self.path == "/ayt":
                return self._javob(200, MIYA.ayt(fel, ayol, matn, ohang, namuna), "audio/wav")

            # oqim: "PCM1" + chastota (uint32 LE), keyin xom PCM16 mono (Content-Length yo'q, HTTP/1.0 - ulanish yopilishi
            # oxiri); har bo'lak tayyor bo'lishi bilan. Sarlavha o'yinga xato matnini ovoz deb olmaslik uchun
            def yoz(b: bytes):
                nonlocal boshlandi
                if not boshlandi:
                    self.send_response(200)
                    self.send_header("Content-Type", "application/octet-stream")
                    self.end_headers()
                    self.wfile.write(struct.pack("<4sI", b"PCM1", SR))
                    boshlandi = True
                self.wfile.write(b)
                self.wfile.flush()

            MIYA.ayt_oqim(fel, ayol, matn, yoz, ohang, namuna)
        except (BrokenPipeError, ConnectionResetError):
            log("o'yin ulanishni uzdi - gap tashlab ketildi")
        except Exception as e:  # o'yin tayyor ovozga qaytadi
            log("xato:", repr(e))
            if not boshlandi:
                try:
                    self._javob(500, repr(e).encode("utf-8"))
                except OSError:
                    pass


def ota_kuzat(pid: int):
    """Ota (o'yin) jarayon tugashini kutadi va serverni to'xtatadi. WSL'da Windows PID ko'rinmaydi - tasklist.exe orqali."""
    if os.name != "nt":
        import subprocess
        while True:
            try:
                r = subprocess.run(["/mnt/c/Windows/System32/tasklist.exe", "/FI", f"PID eq {pid}", "/NH"], capture_output=True, timeout=20)
                if r.returncode == 0 and str(pid).encode() not in r.stdout:
                    log("ota jarayon tugadi - chiqaman")
                    os._exit(0)
            except Exception:
                pass
            time.sleep(5)
    import ctypes
    k32 = ctypes.windll.kernel32
    h = k32.OpenProcess(0x00100000, False, pid)  # SYNCHRONIZE
    if not h:
        log(f"ota jarayon {pid} topilmadi - chiqaman")
        os._exit(0)
    k32.WaitForSingleObject(h, 0xFFFFFFFF)
    log("ota jarayon tugadi - chiqaman")
    os._exit(0)


def stdin_kuzat():
    """stdin yopilguncha kutadi (o'yin wsl.exe ni to'xtatsa yoki quvurni yopsa EOF keladi) va chiqadi."""
    try:
        while sys.__stdin__.buffer.read(4096):
            pass
    except Exception:
        pass
    log("stdin yopildi - chiqaman")
    os._exit(0)


def main():
    global MIYA
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=18600)
    p.add_argument("--ota", type=int, default=0)
    p.add_argument("--log", default="")
    p.add_argument("--muhit", default="")
    p.add_argument("--stdin", action="store_true")
    a = p.parse_args()
    if a.muhit:
        muhit_ornat(Path(a.muhit).expanduser())
    if a.log:
        Path(a.log).parent.mkdir(parents=True, exist_ok=True)
        f = open(a.log, "w", encoding="utf-8", buffering=1)
        sys.stdout = sys.stderr = f
    if a.ota:
        threading.Thread(target=ota_kuzat, args=(a.ota,), daemon=True).start()
    if a.stdin:
        threading.Thread(target=stdin_kuzat, daemon=True).start()
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
    # har sotuvchining o'z ovozi (xulq.json "ovoz.ohang"): izlar fonda oldindan - birinchi gapi kutib qolmasin
    threading.Thread(target=sotuvchi_izlari, args=(m,), daemon=True).start()
    threading.Event().wait()


def sotuvchi_izlari(m: "Miya"):
    yol = PAPKA.parent.parent / "Content" / "Bozor" / "Data" / "xulq.json"
    try:
        sotuvchilar = json.loads(yol.read_text(encoding="utf-8")).get("sotuvchilar", {})
    except (OSError, ValueError) as e:
        log("xulq.json o'qilmadi:", repr(e))
        return
    t0, n = time.time(), 0
    for x in sotuvchilar.values():
        ohang = float((x.get("ovoz") or {}).get("ohang", 0.0))
        namuna = str((x.get("ovoz") or {}).get("namuna", ""))
        if not ohang and not namuna:
            continue
        ism = str(x.get("ism", ""))
        ayol = ism.endswith(" opa") or ism.endswith(" xola")
        # o'yin so'rovi navbatda bo'lsa - u birinchi (bu izlar shoshilinch emas: suhbatdagi sotuvchiniki o'zi tayyorlanadi)
        while m.sorovlar > 0:
            time.sleep(0.05)
        try:
            with m.qulf:
                m.iz(str(x.get("fel", "mehribon")), ayol, ohang, namuna)
            n += 1
        except Exception as e:
            log("ovoz izi xatosi:", ism, repr(e))
    log(f"sotuvchilar ovozi: {n} ta iz, {time.time() - t0:.1f} s")


if __name__ == "__main__":
    main()
