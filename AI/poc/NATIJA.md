# 0-bosqich: kuchsiz kompyuterda lokal AI — o'lchov natijalari (2026-11-12)

**Sharoit:** hamma dastur faqat **4 ta E-yadroda** (Ultra 9 285K, niqob `0xF00`), videokartasiz (`-ng`, `-ngl 0`).
Haqiqiy kuchsiz kompyuter (masalan, 4 yadroli i5-8400) taxminan **2 baravar sekinroq** deb hisoblang.

## 1. Eshitish (STT) — ✅ hal bo'ldi
| Model | Hajm | 12 gap (52.6 s audio) | WER |
|---|---|---|---|
| NVIDIA FastConformer uz, CTC, **q8_0** (CrispASR) | **117 MB** | **1.32 s** (40x real vaqt) | **0%** (0/75 so'z) |
| xuddi shu, q4_k | 70 MB | ~1.2 s | buzuq ("ovchonol" qo'shimchalari) — **ishlatmaslik** |

- 3 soniyalik gap ≈ **0.1 s**. Model yuklash bir martalik (~0.1–0.3 s).
- Diqqat: Common Voice ma'lumoti model o'qitilishida bo'lgan bo'lishi mumkin — 0% optimistik.
  Haqiqiy sinov: bola ovozi, shovqin, arzon mikrofon bilan yozilgan bozor gaplari kerak.
- Litsenziya: CC-BY-4.0 (NVIDIA) — o'yinda "NVIDIA NeMo" deb yozish kifoya. CrispASR — MIT.
- Tinish belgilari modeli (`fireredpunc`) avtomatik yuklanadi va o'zbekchaga mos emas → `--no-punctuation`.

## 2. Miya (LLM) — ⚠️ tezlik yetadi, sifat yo'q → o'qitish shart
18 ta bozor gapi, JSON sxema (grammatika) bilan:
| Model | Qayerda | Niyat to'g'ri | JSON to'g'ri | Tezlik |
|---|---|---|---|---|
| Qwen3-0.6B-Instruct-Uz q8 (640 MB) | 4 E-yadro | 3/18 | 6/18 | prompt ~160 t/s, yozish **~40 t/s** |
| alloma-1B q8 (1.3 GB) | 4 E-yadro | 5/18 | 17/18 | prompt ~90 t/s, yozish **~26 t/s** |
| qwen2.5:7b (Ollama) | taqqoslash uchun | 7/18 | 18/18 | — |

- **Xulosa:** o'zbekcha bozor niyatini umumiy modellar (hatto 7B) yaxshi tushunmaydi. Kichik modelni
  **bozor suhbatlari to'plamida o'qitish (fine-tune)** kerak — tor vazifada 0.6–1B yetarli bo'lishi kutiladi.
- Tezlik yetarli: o'qitilgan model ixcham javob bersa (~20–25 token, JSON bo'shliqlarisiz),
  0.6B da **~0.6–0.8 s**. Tizim prompti keshda turadi (`cache_prompt`), har gapda faqat ~10–20 token o'qiladi.
- alloma tokenizatori llama.cpp konvertorida tanilmadi; regex Llama-3 bilan bir xil, `llama-bpe` qo'yildi
  (`vendor/llama-src/.../conversion/base.py:2449`, faqat lokal nusxa).

## 3. Gapirish (TTS) — ✅ tezlik yetadi, o'zbekcha ovoz yo'q
| Model | Hajm | Bir gap (3–4 s audio) | RTF |
|---|---|---|---|
| Piper VITS "high" (qozoqcha ovoz, faqat tezlik uchun) | 128 MB | 1.1–1.5 s | ~0.34 |

- "medium" Piper ovozi (~60 MB) taxminan 2–3 baravar tezroq bo'ladi.
- O'zbekcha ovozni o'zimiz o'qitamiz: Navoiy TTS (o'qituvchi) → Piper/VITS (shogird).

## Butun zanjir (bitta savol-javob), 4 E-yadro
| Qadam | Hozir | Maqsad (o'qitilgandan keyin) |
|---|---|---|
| STT | 0.1 s | 0.1 s |
| LLM | 2–3 s (o'qitilmagan, uzun JSON) | ~0.7 s |
| TTS | 1.3 s (high) | ~0.5 s (medium) yoki tayyor WAV |
| **Jami** | — | **~1.3 s** (kuchsiz PC'da ~2.5–3 s) |

Qo'shimcha RAM: STT ~0.2 GB + LLM ~0.7 GB + TTS ~0.1 GB ≈ **1 GB**. O'yinga qo'shiladigan hajm ≈ **0.8–1 GB**.

## Keyingi qadamlar
1. Bozor suhbatlari to'plami (minglab bola gapi → niyat + sotuvchi javobi), sheva/xato yozuvlar bilan.
2. Qwen3-0.6B-Uz va alloma-1B ni LoRA bilan o'qitish, 18 gaplik sinovni 200 gapga kengaytirib qayta o'lchash.
3. Navoiy bilan sotuvchi ovozlarini yozish → Piper ovozini o'qitish.
4. O'zimiz yozgan bozor gaplari (bola ovozi, shovqin) bilan STT ni qayta sinash.

## Fayllar
- `yukla.sh` — dastur va modellarni yuklaydi; `olchov.ps1` — 4 E-yadroda vaqt o'lchash
- `miya_sinov.mjs` — LLM sinovi; `wer.mjs` — STT xato foizi
- `natija/` — xom natijalar (git'ga kirmaydi)
