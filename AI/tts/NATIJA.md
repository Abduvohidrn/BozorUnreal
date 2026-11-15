# Navoiy TTS: birinchi sinov (2026-11-12)

**Muhit:** WSL2 Ubuntu 26.04, `~/navoiy` (o'rnatish: [`ornat.sh`](ornat.sh)), Python 3.10 (uv), torch 2.7.1+cu128, RTX 5060 Ti 16 GB.
**Namuna ovoz:** Navoiy demosi `demo/warm_agent.wav` (sintetik ovoz). Navoiy'da tayyor ovozlar yo'q, u har gal namunadan nusxa oladi.
**Ishga tushirish:** `wsl -d Ubuntu -- bash AI/tts/sinov.sh [namuna.wav]` → `natija/*.wav`.

## O'rnatishdagi to'siqlar (ornat.sh da hal qilingan)
| Muammo | Yechim |
|---|---|
| CosyVoice torch 2.3.1 (cu121) so'raydi, u Blackwell (sm_120) da ishlamaydi | torch 2.7.1 cu128 |
| openai-whisper 20231117: `pkg_resources` yo'q, keyin triton<3 bilan to'qnashuv | `setuptools<81` build cheklovi, whisper 20250625 |
| pyworld: WSL'da C++ kompilyator yo'q, Python 3.10 uchun wheel yo'q | o'rinbosar modul ([`pyworld.sh`](pyworld.sh)). Faqat o'qitishda kerak |

## O'lchovlar
| # | Uslub | Audio, s | Yozish, s | RTF | STT orqali qaytgan matn |
|---|---|---|---|---|---|
| 1 | mehribon | 5.96 | 5.00 | 0.84 | Assomu alaykum bolam keling nima olasiz pomidor bodring hammasi yangi |
| 2 | mehribon | 3.92 | 2.01 | 0.51 | Siz odobli bola ekansiz oʻn ming soʻmdan beraman |
| 3 | xotirjam | 2.64 | 1.57 | 0.59 | Bir kiloomidor oʻn ikki ming besh yuz soʻm |
| 4 | jahldor | 3.04 | 1.68 | 0.55 | Narxi shu tushmaydi boshqa joyda qimmatroq |
| 5 | qo'rqqan | 3.88 | 1.92 | 0.50 | Ha tez ayting nima olasiz nechta tezroq bolam |
| 6 | kinoyali | 3.36 | 3.98 | 1.18 | Voy bundan arzon bersam oʻzim zarar qilaman-ku |
| 7 | xotirjam | 5.12 | 8.26 | 1.61 | Mana ikki yarim kilo kartoshka jami oʻn ikki ming besh yuz soʻm boʻldi |
| 8 | mehribon | 6.12 | 13.42 | 2.19 | Voy rostan ham Uzr bolam mana bir kilo oʻn ming soʻm |

- Model yuklanishi 10.5 s, VRAM eng ko'pi bilan **3.4 GB**.
- **Tushunarlilik:** o'yin STT'si (FastConformer uz) 8 gapning hammasini deyarli so'zma-so'z tanidi, sonlar ham to'g'ri.
- Oxirgi gaplar sekinroq yozildi (RTF 1.2–2.2). Sinov paytida boshqa Unreal muharriri ham ochiq edi. Oldindan yozish uchun bu muhim emas.
- Ovoz sifati, uslublar farqi va 8-gapning cho'zilganligi (6 s) **quloq bilan tekshirilishi kerak**.

## O'yin uchun gaplar (2-qadam)

**Namuna ovozlar** (har biridan Navoiy nusxa oladi):
- `ayol` — `CosyVoice/asset/zero_shot_prompt.wav` (F0 ~245 Hz);
- `erkak` — `cross_lingual_prompt.wav` (F0 ~143 Hz).
- Ikkalasi CosyVoice repo'sidan, Apache-2.0. Barcha Navoiy demolari ham shu ayol ovozida.
- Aisha'ning ovoz to'plamlari (`uzbek-tts-corpus-v1`, `uzbek-emotion-tts`) yopiq, ularga kirish ruxsati kerak.
- O'yindagi mavjud chaqiriq kliplarining (`sot_*`) manbasi bu kompyuterda yo'q. Kod izohida ular "haqiqiy yozuv" deb yozilgan, shuning uchun ularni nusxalamadik.

**Bo'laklash** (o'yin va skript bir xil qoida bilan ishlaydi: `UBozorTayyorJavob::Yoz` ↔ [`gaplar_yoz.py`](gaplar_yoz.py)):

| Bo'lak | Kalit | Uslub |
|---|---|---|
| Qolip matni (o'rinlar orasida) | `G_<md5[:8]>` | sotuvchi fe'li: mehribon → mehribon, qattiq → xotirjam, shoshqaloq → qo'rqqan (1.1× tez), ayyor → kinoyali |
| Son 12 500 | `S_12m` + `S_500` | neytral (xotirjam) |
| Miqdor 2,5 | `M_25` | neytral |
| Mol / birlik | `P_<id>` / `B_<birlik>` | neytral |

- 720 bo'lak × 2 ovoz = **1440 fayl**.
- Personalar: a1–a3 ayol ovozida, e1–e2 erkak ovozida gapiradi. Farqi pitch'da: a2 1.07, a3 0.93, e2 0.92.
- Bot bilan tekshirildi: o'yin so'ragan 73 xil bo'lakning hammasi skript ro'yxatida bor, ya'ni kalitlar mos.

**Sifat nazorati:**
- Har bo'lak o'yin STT'si bilan matnga qaytariladi va asl matn bilan solishtiriladi.
- O'xshashlik 0.9 dan past bo'lsa, boshqa seed bilan qayta yoziladi (5 martagacha) va eng yaxshi natija qoladi.
- Natija `gaplar/sifat.tsv` ga yoziladi.
- Nazoratsiz birinchi sinovda 30 bo'lakdan 22 tasi aniq mos edi. Xatolar asosan qisqa bo'laklar va kinoyali uslubda chiqdi.

**Unreal'ga olib kirish:**
- `UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/import_gaplar.py` fayllarni `/Game/Bozor/Audio/Gaplar` ga import qiladi.
- `ABozorSotuvchiOvozi::SayGap` bo'laklarni ketma-ket chaladi. Bitta bo'lak ham topilmasa, gap ovozsiz qoladi.
- Bo'laklar o'yinda nomi bilan yuklanadi, shuning uchun papka `DefaultGame.ini` da `DirectoriesToAlwaysCook` ga qo'shilgan (aks holda paketga tushmaydi).

## Keyingi
1. Quloq bilan baholash: uslublar sotuvchi fe'liga mos keladimi, bo'laklar orasidagi ulanish sezilyaptimi.
2. Haqiqiy personalar uchun namuna ovozlar (rozilik bilan yozilgan yoki Aisha to'plami ruxsati bilan). Ovozni almashtirish uchun `NAMUNA` ni o'zgartirib, skriptni qayta ishga tushirish kifoya.
