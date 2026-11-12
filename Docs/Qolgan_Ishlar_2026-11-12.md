# Bozor o'yini: qolgan ishlar va ularni qanday qilamiz

**Sana:** 2026-11-12
**Asos:** 2026-11-11 dagi reja ("o'qituvchi va shogird", 0–5-bosqichlar), [AI hisoboti](AI_Hisobot_2026-11-12.md), [0-bosqich o'lchovlari](../AI/poc/NATIJA.md), [Tarozi senariysi](Senariy_Tarozi.md).

**Maqsad (o'zgarmagan):** bola sotuvchi bilan o'zbekcha ovoz orqali savdo qiladi, sotuvchi ham o'zbekcha ovoz bilan javob beradi. Hammasi o'yin ichida lokal ishlaydi: internet kerak emas, videokarta kerak emas, Python ham kerak emas. O'yin kuchsiz kompyuterda ham ishlashi kerak.

**Asosiy qoida:** katta modellar (Navoiy, rubaiSTT, katta LLM) faqat ishlab chiquvchining kompyuterida "o'qituvchi" bo'ladi. O'yin ichiga faqat kichik "shogird" modellar kiradi. Narx, pul va qaytimni har doim `UBozorSuhbat` hisoblaydi, AI esa faqat tushunadi va gapiradi.

---

## 1. Qayerda turibmiz

| Bosqich (kechagi reja) | Holat | Natija |
|---|---|---|
| 0. Kuchsiz kompyuterda isbot (STT + LLM + TTS) | ✅ Tugadi | STT 0.1 s, LLM tez lekin o'qitilmagan holda yaroqsiz, Piper tez ([NATIJA.md](../AI/poc/NATIJA.md)) |
| "O'qitishsiz yo'l": mikrofon + qoida bilan tushunish | ✅ O'yinga ulandi | V tugmasi, CrispASR + FastConformer uz, `Bozor.Tushun` 49/49 |
| Tarozi bosqichi + natija statistikasi | ✅ Qilindi | Ko'p/kam tortish, aytish yoki indamaslik, xaridlar jadvali |
| "O'qitishsiz yo'l": sotuvchi **ovozi** (tayyor gaplar) | ⏳ Qilinmagan | Sotuvchi hozir matn bilan javob beradi |
| 1. Suhbat to'plami va miyani o'qitish | ⏳ Qilinmagan | — |
| 2. Navoiy bilan ovoz yozish, Piper ovozini o'qitish | ⏳ Qilinmagan | — |
| 3. Eshitish (STT) ni haqiqiy sharoitda sinash | 🟡 Qisman | Sizning ovozingiz bilan ~34 gap sinaldi. Bola ovozi va shovqin bilan sinalmagan |
| 4. Unreal: AI miya, ovoz chiqarish, "o'ylab turish" | 🟡 Qisman | Mikrofon tayyor. AI miya va sotuvchi ovozi yo'q |
| 5. Kuchsiz kompyuterlarda sinov va tarqatish | ⏳ Qilinmagan | — |

---

## 2. Darhol qilinadigan mayda ishlar (bir kunlik)

| # | Ish | Nega | Qanday |
|---|---|---|---|
| 2.1 | `Docs/AI_Hisobot_2026-11-12.md` va shu hujjatni commit + push qilish | GitHub'da yo'q | `git add Docs/*.md`, commit, push |
| 2.2 | NPC migratsiyasi zaxirasini `Saved/` dan olib chiqish | `Saved/` git'ga kirmaydi, fayl yo'qolsa migratsiya ishi ham yo'qoladi | `Saved/Zaxira/BozorNpc.cpp.migratsiya-oldi` → `Arxiv/` (yoki alohida `npc-migratsiya` branch) |
| 2.3 | Tarozida "indamaslik" yo'lini qo'lda sinash | Bot har doim xatoni aytadi, ko'p/kam baholar ekranda ko'rilmagan | 1-darajada "Mayli, rahmat" ni bosing, natija jadvalida to'q sariq "ko'p" chiqishi kerak |
| 2.4 | Botga `-Indamas` rejimini qo'shish | 2.3 ni avtomatik sinash uchun | `BotQadami` da `TaroziTekshir` bosqichida flag bo'lsa `TaroziRozi` tanlanadi |
| 2.5 | `Bozor_Oyin.bat` / `Bozor_Muharrir.bat` yo'llari | Hali yo'q `C:\Program Files\Epic Games\UE_5.8` ni ko'rsatadi | Dvigatel yo'lini `%UE%` o'zgaruvchisiga olish yoki joriy o'rnatishga moslash |
| 2.6 | Windows mikrofon darajasi | Xom signal cho'qqisi 0.05–0.1, juda past | Sozlamalar → Tovush → Mikrofon → 80–100% |

---

## 3. A-bosqich: sotuvchi ovozi (tayyor gaplar, Navoiy bilan oldindan yozish)

**Nega birinchi:** o'qitish talab qilmaydi, o'yinga darhol "jonli" his beradi va kuchsiz kompyuterga hech qanday yuk qo'shmaydi (oddiy WAV/OGG chalinadi). Keyingi bosqichlarda ham shu ovozlar ishlatiladi.

### 3.1. Hozir nima bor
- `Content/Bozor/Audio/Sotuvchi/` da 138 ta klip bor: `sot_<tur>_<bo'lim>_<persona>_<nn>` (chaqiriq, salom, savdo, rahmat).
- Personalar: ayollar `a1–a3`, erkaklar `e1–e2`. Har sotuvchiga bitta persona biriktiriladi ([BozorOvoz.h](../Source/BozorUnreal/BozorOvoz.h)).
- Bu kliplar faqat umumiy gaplar (baqiriq, salom). Suhbatdagi aniq javoblar ("12 000 so'm bo'ladi, bolam") hali ovozlanmagan.

### 3.2. Qadamlar
1. **Navoiy muhitini o'rnatish** (sizning kompyuteringizda):
   - WSL2 (Ubuntu) + CUDA 12.8 + yangi PyTorch (RTX 5060 Ti Blackwell uchun). CosyVoice asosan Linux uchun yozilgan, shuning uchun Windows'da to'g'ridan-to'g'ri urinmaymiz.
   - CosyVoice kodi + CosyVoice2-0.5B + `aisha-org/navoiy-tts` checkpoint.
   - Birinchi o'lchov: bitta gapni yozish vaqti, VRAM, sifat.
2. **Har persona uchun ovoz namunasi.** 10–15 soniyalik toza yozuv kerak, ovoz egasining **roziligi bilan** (o'zingiz, tanishlar). Yoki Navoiy'ning o'z ovozlari ishlatiladi.
3. **Fe'l → uslub bog'lanishi:** Mehribon → *iliq*, Qattiq → *jahldor*, Shoshqaloq → *asabiy*, Ayyor → *kinoyali*. Natija oynasidagi "iliq/sovuq" kayfiyat ham uslubni o'zgartirishi mumkin.
4. **Gaplarni bo'laklarga ajratish.** Javoblarda o'zgaruvchilar bor (`{narx}`, `{summa}`, `{miqdor}`, `{birlik}`, `{mahsulot}`, `{qaytim}`). Har narx uchun butun gapni yozib bo'lmaydi (narxlar 500 dan 130 000 gacha). Shuning uchun:
   - qolip qismlari alohida yoziladi: "Mana," / "bo'ladi, bolam";
   - sonlar so'z bo'laklaridan yig'iladi: "o'n ikki" + "ming" + "besh yuz" + "so'm";
   - mahsulot nomlari va birliklar ham alohida yoziladi.
   - **Muhim:** bo'laklar alohida aytilsa, ohangi uzuq chiqadi. Shuning uchun har bo'lak "tashuvchi gap" ichida yoziladi (masalan, "Bu o'n ikki ming so'm bo'ladi"), keyin kerakli qism kesib olinadi.
5. **Gap qoliplarini kod ichidan JSON'ga ko'chirish.** Hozir asosiy javoblar `UBozorTayyorJavob::Yoz` ichidagi `TMap` da turibdi ([BozorSotuv.cpp](../Source/BozorUnreal/BozorSotuv.cpp)). Ular `sotuv.json` ga ("gaplar" bo'limi) ko'chiriladi. Shunda yozish skripti ham, o'yin ham bitta ro'yxatni o'qiydi.
6. **Yozish skripti:** `AI/tts/yoz.py` (WSL'da). U `sotuv.json` dan gaplar va bo'laklar ro'yxatini tuzadi, Navoiy bilan har persona × uslubda yozadi, ovoz balandligini tenglaydi va boshidagi va oxiridagi jimlikni kesadi. Natija mavjud `import_audio.py` orqali `Content/Bozor/Audio/Gaplar/` ga import qilinadi.
7. **Unreal tomoni:** `Yoz` matn bilan birga **bo'laklar ro'yxatini** ham qaytaradi (masalan, `FBozorGapBolak` massivi). `ABozorSotuvchiOvozi` ga `SayGap(Sotuvchi, Bolaklar)` qo'shiladi: u bo'laklarni ketma-ket, sotuvchining og'zida (3D) chaladi. Subtitr (matn) qoladi. Bo'lak topilmasa, o'sha gap jim qoladi va jurnalga yoziladi.

### 3.3. Tekshirish
- Bot o'ynaganda `Sotuv_suhbat.txt` da "ovoz bo'lagi topilmadi" qatori bo'lmasligi kerak.
- 10 ta tasodifiy narxni quloq bilan tinglash: bo'laklar orasidagi uzilish sezilmasligi kerak.

### 3.4. Hajm va vaqt
- Taxminan 5 persona × ~150 gap/bo'lak × ~2 s → OGG'da **~30–60 MB**.
- Vaqt: muhitni o'rnatish 1 kun (xavfli qism shu), skript va import 1–2 kun, Unreal tomoni 1 kun.

### 3.5. Xavflar
- CosyVoice WSL'da ham chiqmasa → zaxira: Piper/VITS ovozini to'g'ridan-to'g'ri o'qitish (5-bo'lim) yoki ovozlarni odamga yozdirish.
- Navoiy narxlarni noto'g'ri o'qishi mumkin → sonlarni oldindan so'z bilan yozib beramiz (normalizator ishlatmaymiz).

---

## 4. B-bosqich: miyani o'qitish (LoRA)

**Nega kerak:** 0-bosqichda tayyor modellar o'zbekcha bozor gaplarining atigi 3–7 tasini to'g'ri tushundi (18 tadan). Hozirgi qoidalar asosiy gaplarni yaxshi tushunadi, lekin kutilmagan gapga ("Amaki, bu pomidor qayerdan?") javob bera olmaydi.

**Qaror nuqtasi:** o'qitilgan model 200 ta yangi gapda qoidalardan **sezilarli yaxshi** bo'lmasa, uni o'yinga qo'shmaymiz. Bu holda o'yin ~1 GB yengilroq qoladi.

### 4.1. Suhbat to'plami (eng ko'p vaqt oladigan qism)
- **Format (JSONL):** har qator = holat + bola gapi → niyat + (ixtiyoriy) erkin javob:
  ```json
  {"holat": {"bosqich": "NarxAytildi", "mollar": ["pomidor","bodring"], "tanlangan": "pomidor", "narx": 12000},
   "gap": "amaki o'n mingdan bering iltimos",
   "niyat": {"harakat": "Savdolashish", "mol": "pomidor", "qiymat": 10000, "odob": 1, "salom": false}}
  ```
  `niyat` maydonlari `FBozorTushuncha` bilan bir xil. Javob turi `EBozorJavob` dan tanlanadi. Erkin matn faqat "boshqa" (mavzudan tashqari) gaplar uchun kerak.
- **Manbalar:**
  1. `Bozor.Tushun` sinovidagi 49 ta gap (tayyor, tekshirilgan);
  2. `Saved/Quloq/jurnal.tsv` dagi haqiqiy mikrofon yozuvlari;
  3. katta model (Ollama'dagi qwen2.5:7b / gemma4:12b) yozgan variantlar: odobli, qo'pol, sheva, xato talaffuz, so'z bilan aytilgan sonlar;
  4. STT xatolariga o'xshatib buzilgan nusxalar ("pamidor", "qanch", "raxmat").
- **Hajm:** 3–10 ming juftlik. 200 tasi **o'qitishga kirmaydi**, faqat sinov uchun ajratiladi. Bu 200 tani siz qo'lda tekshirasiz.
- **Avtomatik tekshirish:** har bir juftlik hozirgi qoidalardan (`Tushun`) ham o'tkaziladi. Ular kelishmagan joylar qo'lda ko'rib chiqiladi.

### 4.2. O'qitish
- Muhit: WSL2 + Unsloth (yoki PEFT) + RTX 5060 Ti.
- Nomzodlar: **Qwen3-0.6B-Uz** (640 MB, ~40 token/s) va **alloma-1B** (1.3 GB, ~26 token/s).
- Har biriga 15–60 daqiqa. Keyin GGUF'ga o'tkaziladi: q8_0 va q4_k_m sinab ko'riladi. Ehtiyot bo'lish kerak: STT'da q4 buzuq chiqqan edi.
- Javob ixcham bo'lishi kerak (JSON bo'shliqlarisiz, ~20–25 token). Shunda 4 E-yadroda ~0.7 s ketadi.

### 4.3. Sinov
- `AI/poc/miya_sinov.mjs` 18 gapdan 200 gapga kengaytiriladi, GBNF grammatika bilan.
- **Maqsadlar:**
  - niyat to'g'ri ≥ 90%;
  - JSON to'g'ri ≥ 98%;
  - son to'g'ri ≥ 95%;
  - 4 E-yadroda < 0.8 s.
- Taqqoslash: **xuddi shu 200 gapda hozirgi qoidalar necha foiz topadi** (avval o'lchanmagan, bu muhim).

---

## 5. C-bosqich: miyani Unreal'ga ulash

### 5.1. Usul
Mikrofon qismi bilan bir xil yo'l tanlanadi: **llama-server.exe alohida jarayon**, o'yin uni o'zi ishga tushiradi va `127.0.0.1` orqali so'rov yuboradi. `libllama` ni dvigatelga qo'shib yig'ish keyinroq, kerak bo'lsa qilinadi.
- Fayllar: `AI/miya/` (llama-server.exe, model.gguf, grammatika.gbnf). `Build.cs` dagi `RuntimeDependencies` ga qo'shiladi, git'ga kirmaydi, `yukla.sh` yuklab beradi (`AI/stt` kabi).
- 2 yadro bilan cheklanadi (`-t 2`). Tizim prompti keshda turadi (`cache_prompt`).

### 5.2. Kod o'zgarishlari
1. **`UBozorAiMiya : UBozorSotuvchiMiya`** — `Tushun` ni HTTP orqali so'raydi, JSON'ni `FBozorTushuncha` ga o'giradi.
2. **Asinxron yo'l.** Hozir `Tushun()` sinxron, javobni darhol qaytaradi. Shuning uchun `TushunAsync(Gap, Holat, Callback)` qo'shiladi. `ABozorOyinchi` mikrofondan matn kelganda shu yo'lni chaqiradi. Tugmalar o'zgarmaydi, ularning niyati tayyor.
3. **Gibrid:** avval qoidalar ishlaydi. Ular ishonchli topsa (aniq son, mol, harakat), AI'ga murojaat qilinmaydi. Faqat "Tushunmadi" yoki shubhali holatda AI chaqiriladi. Shu tarzda ko'p gaplar 0 ms da hal bo'ladi.
4. **Xavfsizlik:** AI qaytargan son gapning o'zida bo'lishi kerak (so'z yoki raqam bilan). Bo'lmasa, AI natijasi tashlab yuboriladi. Narxni AI hech qachon belgilamaydi.
5. **Kutish:** javob kelguncha sotuvchi "o'ylab turadi" (yangi `V_Think` harakati: bosh qimirlatish, "hmm" klipi). 2.5 s timeout bo'lsa, qoidalarga qaytiladi ("Nima dedingiz, bolam?").
6. **Sozlama:** `sotuv.json` ga quyidagi qo'shiladi:
   ```json
   "ai": { "eshit": true, "miya": true, "timeout": 2.5, "yadro": 2 }
   ```
   AI fayllari yo'q bo'lsa yoki kompyuter sekin bo'lsa, avtomatik o'chadi.

### 5.3. Sinov
- `Bozor.Miya` avtomatik sinovi: 200 ta gap o'yin ichidan, vaqt bilan.
- Bot rejimiga erkin gap qo'shish (`-DarajaTest -Erkin`): bot tugma o'rniga matn yuboradi.

---

## 6. D-bosqich: jonli ovoz (Piper shogird) — ixtiyoriy

**Faqat B/C-bosqichlar AI'ning erkin javoblarini olib kelsa kerak bo'ladi.** Tayyor gaplar A-bosqichda yozib bo'linadi.
- Navoiy har persona ovozida 1–3 soat gap yozadi, matnlarini 4-bo'limdagi to'plamdan olamiz.
- Har persona uchun Piper/VITS "medium" ovozi (~60 MB) o'qitiladi. U har gapni 0.3–0.5 s da yozadi.
- **Litsenziya:** Piper odatda espeak-ng (GPL) ishlatadi. Lotin yozuvidagi o'zbekcha deyarli fonetik bo'lgani uchun **harflar bilan ishlaymiz, espeak'siz**.
- O'yinda: `piper.exe` alohida jarayon (STT kabi) yoki ONNX + Unreal NNE.

---

## 7. Eshitish (STT) ni mustahkamlash

| Ish | Qanday |
|---|---|
| Haqiqiy sinov | 10–13 yoshli bolalar ovozi, bozor shovqini (`Audio/Fon` ostida), arzon mikrofon. 50–100 gap yozib, `AI/poc/wer.mjs` bilan WER o'lchanadi |
| Jurnal tahlili | `Saved/Quloq/jurnal.tsv`: qaysi so'zlar ko'p xato tanilyapti. Ular `Tushun` dagi o'xshash so'z tuzatishga qo'shiladi |
| Mikrofon sozlash | Birinchi ishga tushirishda "Assalomu alaykum deng" sinovi: daraja past bo'lsa, ogohlantirish chiqadi |
| Avtomatik to'xtash (VAD) | Ixtiyoriy: jimlik 0.6 s davom etsa, V qo'yilmasa ham yozish to'xtaydi |
| Windows ruxsati | Mikrofon ruxsati yo'q bo'lsa, ekranda tushunarli xabar chiqadi |

Kerak bo'lsa: FastConformer'ni bozor gaplari bilan moslash (fine-tune). Bu faqat WER > 15% chiqsa qilinadi.

---

## 8. Tarqatish va kuchsiz kompyuter

1. **Paketlash:** Win64 Shipping build. `AI/stt` (va keyinroq `AI/miya`, `AI/tts`) paket ichiga tushganini va paketlangan o'yinda mikrofon ishlashini tekshirish.
2. **Mualliflar oynasi (majburiy):**
   - "NVIDIA NeMo" (FastConformer, CC-BY-4.0);
   - CrispASR (MIT);
   - "Built with Llama" (alloma tanlansa);
   - Navoiy / rubaiSTT (Apache-2.0).
   Litsenziya fayllari ham paket bilan beriladi.
3. **Grafika:** Scalability "Low" profili. Kuchsiz kompyuterda haqiqiy chegara AI emas, Unreal 5 ning o'zi bo'ladi.
4. **Minimal kompyuterda sinov:** 4 yadroli CPU, 8 GB RAM, integratsiyalashgan grafika. Do'stlarning kompyuterida yoki sizning kompyuteringizda E-yadrolar bilan cheklab.
5. **Hajm byudjeti:** o'yin + STT 140 MB + ovozlar 30–60 MB + (ixtiyoriy) miya 0.6–1.3 GB.

---

## 9. NPC migratsiyasi (AI'dan alohida)

- `BozorNpc.cpp` dagi tugallanmagan migratsiya (Citizen tuzilishiga ko'chish) NPC'larni ikki tanali qilib ko'rsatgan edi. Shu sababli `LinkMetaHuman()` eski holatiga qaytarildi.
- **Qanday davom ettiramiz:** alohida branch ochiladi, zaxiradagi kod qaytariladi, xarita (`Bozor.umap`) ham yangi tuzilishga o'tkaziladi (Python commandlet bilan), ikki tana muammosi topiladi. Keyin bot va avtobus intro sinovlari o'tkaziladi.
- Bu ishni AI bosqichlari bilan bir vaqtda qilmagan ma'qul, chunki ikkalasi ham `BozorNpc` / `BozorOyinchi` ga tegadi.

---

## 10. Tavsiya etilgan tartib

| Tartib | Ish | Taxminiy vaqt | Natijada |
|---|---|---|---|
| 1 | 2-bo'lim: mayda ishlar | 0.5–1 kun | Hammasi GitHub'da, zaxira xavfsiz |
| 2 | A: sotuvchi ovozi (Navoiy → tayyor WAV) | 3–5 kun | Sotuvchi o'zbekcha **gapiradi** |
| 3 | 7: STT'ni bola ovozi bilan sinash | 1–2 kun (yozuvlar bilan) | Haqiqiy aniqlik ma'lum bo'ladi |
| 4 | B: to'plam + o'qitish + 200 gaplik sinov | 5–10 kun | Miya kerakmi, yo'qmi, raqam bilan isbotlanadi |
| 5 | C: miyani Unreal'ga ulash (B o'tsa) | 3–4 kun | Kutilmagan gaplarga ham javob |
| 6 | D: Piper jonli ovoz (C erkin javob bersa) | 3–5 kun | AI'ning yangi gaplari ham ovozli |
| 7 | 8: paketlash va kuchsiz kompyuterda sinov | 2–3 kun | Tarqatishga tayyor |
| — | 9: NPC migratsiyasi | alohida | — |

**Eng birinchi qadam:** 2-bo'lim, keyin Navoiy'ni WSL2'da ishga tushirib, bitta gapni bitta persona ovozida yozib ko'rish. Bu A-bosqichdagi eng xavfli qism.

---

## 11. Sizdan kerak bo'ladigan qarorlar

1. **Sotuvchi ovozlari:** Navoiy'ning o'z ovozlarimi, yoki tanishlaringiz yozib beradimi (rozilik bilan, har biri 10–15 s)?
2. **Miya:** B-bosqichni qilamizmi, yoki A + qoidalar bilan to'xtab, o'yin mazmuniga (yangi darajalar, mollar) o'tamizmi?
3. **WSL2** o'rnatishga ruxsat (Navoiy va o'qitish uchun kerak).
4. **Bola ovozi bilan sinov:** yozuv beradigan bolalar bormi (ota-onasining roziligi bilan)?
