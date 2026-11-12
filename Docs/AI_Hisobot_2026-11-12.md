# Bozor o'yiniga lokal AI ulash: hisobot

**Sana:** 2026-11-11 / 12
**Maqsad:** o'yinchi sotuvchi bilan o'zbek tilida **ovoz orqali** jonli savdo qiladi. Sotuvchi ham o'zbekcha javob beradi. Barcha AI o'yin ichida, lokal ishlaydi: o'yinchi faqat o'yinni o'rnatadi, internet ham, videokarta ham kerak emas. O'yin iloji boricha kuchsiz kompyuterlarda ishlashi kerak.

---

## 1. Qisqacha natija

| Qism | Holat | Izoh |
|---|---|---|
| **Eshitish (STT, mikrofon → matn)** | ✅ O'yinga ulandi | V bosib turib gapiriladi. Har gapga ~0.12–0.15 s ketadi, faqat CPU ishlatiladi |
| **Tushunish (miya)** | ✅ Qoidalar asosida ishlaydi | Erkin gap, so'z bilan aytilgan sonlar, xato talaffuzlar tushuniladi. 49/49 sinov o'tdi |
| **Gapirish (TTS, sotuvchi ovozi)** | ⏳ Hali qilinmagan | Sotuvchi hozircha matn bilan javob beradi |
| **O'qitilgan AI miya (LLM)** | ⏳ Keyingi bosqich | Tayyor modellar o'zbekcha bozor gaplarini yomon tushunishi o'lchandi, o'qitish kerak |

---

## 2. Tadqiqot: qaysi AI'lar mos keladi

### Navoiy va Ruboiy
- **Navoiy** — [aisha-org/navoiy-tts](https://huggingface.co/aisha-org/navoiy-tts): o'zbekcha **ovoz chiqaruvchi** model (TTS). CosyVoice2-0.5B asosida, Apache-2.0. 10 xil uslubi bor (iliq, jahldor va boshqalar) va ovozni nusxalay oladi. **CUDA (NVIDIA) talab qiladi**, shuning uchun o'yinchi kompyuterida ishlamaydi. U ishlab chiqish paytida, sizning kompyuteringizda ishlatiladi.
- **Ruboiy** — [islomov/rubaistt_v2_medium](https://huggingface.co/islomov/rubaistt_v2_medium): o'zbekcha **nutqni matnga** aylantiruvchi model (STT). Whisper Medium asosida, WER ~17%, Apache-2.0.
- Ikkalasi ham LLM emas, ya'ni "o'ylamaydi". Shuning uchun sotuvchining "miyasi" alohida kerak.

### Kimi K3 (`kimi-k3-in-c`)
Repo yuklab olinmasdan to'liq o'rganildi. **O'yin uchun yaramaydi:**
- hajmi 1.56 TB + 109 GB, jami ~1.7 TB disk kerak;
- faqat CPU'da ishlaydi va har bir **token uchun 20–30 soniya** ketadi (32 GB RAM'da);
- HTTP server rejimi yo'q.

Kimi oilasining kichikroq modellari ham ko'rib chiqildi: Kimi-Linear-48B-A3B va Moonlight-16B-A3B. Ular ham kuchsiz kompyuter uchun og'ir.

### Asosiy qaror
Maqsad aniqlashtirildi: o'yin **internetga joylanadi va kuchsiz kompyuterlarda ishlashi kerak**. Shundan kelib chiqib, tuzilish quyidagicha tanlandi:
- **Katta modellar "o'qituvchi" bo'ladi** (Navoiy, rubaiSTT, katta LLM) va faqat ishlab chiquvchining kompyuterida ishlaydi.
- **O'yin ichiga kichik "shogird" modellar kiradi**: Python'siz, CUDA'siz, faqat CPU'da ishlaydi.

---

## 3. 0-bosqich: kuchsiz kompyuterda o'lchovlar

Hamma dastur **faqat 4 ta E-yadroda** (Ultra 9 285K), videokartasiz ishga tushirildi. To'liq hisobot: [`AI/poc/NATIJA.md`](../AI/poc/NATIJA.md).

| Vazifa | Model | Hajm | Natija |
|---|---|---|---|
| STT | NVIDIA FastConformer uz CTC **q8_0** (CrispASR) | 117 MB | 40× real vaqtdan tez, Common Voice'da **0% xato** |
| STT | xuddi shu model, q4_k | 70 MB | ❌ buzuq natija beradi, ishlatilmaydi |
| LLM | Qwen3-0.6B-Uz | 640 MB | ~40 token/s, niyatni **3/18** to'g'ri topdi |
| LLM | alloma-1B (GGUF'ga o'zimiz o'tkazdik) | 1.3 GB | ~26 token/s, **5/18** |
| LLM | qwen2.5 7B (taqqoslash uchun) | — | **7/18** |
| TTS | Piper VITS (tezlik uchun qozoqcha ovoz) | 128 MB | bir gapga 1.1–1.5 s |

**Xulosa:** STT va TTS tezligi kuchsiz kompyuter uchun yetadi. Tayyor LLM'lar esa o'zbekcha bozor niyatini yomon tushunadi, shuning uchun ularni o'qitish (fine-tune) kerak bo'ladi.

Muhim topilma: [o'zbekcha STT reytingi](https://raw.githubusercontent.com/NavAI-pro/uzbek-text-norm/main/LEADERBOARD_ALL.md). Unga ko'ra FastConformer CTC aniqlikda rubaiSTT'ga yaqin, lekin undan ancha kichik va tez.

---

## 4. O'yinga qilingan o'zgarishlar

### 4.1. Erkin gapni tushunish: `UBozorTayyorJavob::Tushun` ([BozorSotuv.cpp](../Source/BozorUnreal/BozorSotuv.cpp))
- **So'z bilan aytilgan sonlar:** "o'n ikki ming besh yuz" → 12500, "ikki yuz ming" → 200000, "ikkita" → 2 dona. Qo'shimchali shakllar ham tanilyapti ("mingdan", "o'nga"). "uchun", "biroz" kabi so'zlar esa son deb olinmaydi.
- **Narx taklifi:** "o'n mingdan bering" savdolashish deb tushuniladi.
- **Bir harf xatosi:** "pamidor", "qanch", "arzanroq", "raxmat", "aleykum" kabi shakllar ham tanilyapti.
- **Odob:** "aka", "kechirasiz", "uzr" odobli gap, "tez bo'l" qo'pol gap deb olinadi. Bitta xato ham tuzatildi: "Hoy," vergul bilan yozilganda tanilmay qolardi.

### 4.2. Mikrofon: `UBozorQuloq` ([BozorQuloq.h](../Source/BozorUnreal/BozorQuloq.h), [BozorQuloq.cpp](../Source/BozorUnreal/BozorQuloq.cpp))
- **V** bosib turiladi → "Gapiring... ||||" chiqadi → tugma qo'yiladi → "Tushunilmoqda..." → matn sotuvchiga uzatiladi.
- Ovoz yozish uchun Unreal'ning `FAudioCaptureSynth` sinfi ishlatiladi.
- Stereo mikrofonning **eng baland kanali** olinadi.
- 48 kHz → 16 kHz ga **past chastota filtrli** (Hann oynali sinc) resemplash qilinadi.
- Suhbat davomida mikrofon fonda ochiq turadi. Xotirada faqat oxirgi 0.5 s saqlanadi va hech qayerga yuborilmaydi. Shu tufayli gapning boshi yo'qolmaydi.
- Tugma qo'yilgandan keyin yana 0.35 s yoziladi, oxirgi bo'g'in kesilib qolmasligi uchun.
- STT (CrispASR + FastConformer uz) alohida jarayonda, fon oqimida ishlaydi va o'yin qotmaydi.
- Har bir yozuv va STT natijasi `Saved/Quloq/jurnal.tsv` ga yoziladi.
- Konsol buyrug'i `QuloqSinov <fayl>` mikrofonsiz sinash uchun.

### 4.3. O'yinchi ([BozorOyinchi.cpp](../Source/BozorUnreal/BozorOyinchi.cpp))
- V tugmasi qo'shildi, ekranga holat yozuvi chiqadi.
- Suhbat boshlanganda mikrofon yoqiladi, tugaganda o'chiriladi.

### 4.4. Build va tarqatish ([BozorUnreal.Build.cs](../Source/BozorUnreal/BozorUnreal.Build.cs))
- `AudioCaptureCore` va `AudioCapture` modullari qo'shildi.
- `AI/stt/` fayllari (crispasr.exe, openblas.dll, stt-uz.gguf, litsenziyalar) o'yin bilan birga paketlanadi (`RuntimeDependencies`).
- Bu fayllar git'ga kirmaydi. Ularni [`AI/stt/yukla.sh`](../AI/stt/yukla.sh) yuklab beradi va SHA256 yig'indisini tekshiradi.

### 4.5. Avtomatik sinovlar ([BozorSotuvTest.cpp](../Source/BozorUnreal/BozorSotuvTest.cpp))
- `Bozor.Tushun`: 49 ta gap tekshiriladi (niyat, son, mol, odob). **49/49.**
- `Bozor.Quloq`: o'yin ichidan STT ishga tushadi va o'zbekcha matn (`ʻ` belgisi bilan) buzilmasdan keladi. **~150 ms.**
- Ishga tushirish: `UnrealEditor-Cmd.exe BozorUnreal.uproject -ExecCmds="Automation RunTests Bozor.; Quit" -unattended -nullrhi`

### 4.6. NPC ko'rinishi tuzatildi ([BozorNpc.cpp](../Source/BozorUnreal/BozorNpc.cpp))
`BozorNpc.cpp` da avvaldan commit qilinmagan, tugallanmagan migratsiya bor edi. Kompilyatsiyadan keyin u NPC'larni ikki tanali qilib ko'rsatdi. `LinkMetaHuman()` funksiyasi oxirgi commit holatiga qaytarildi, chunki xarita hali `CitizenPose` tuzilishida. Eski versiya zaxirada saqlandi: `Saved/Zaxira/BozorNpc.cpp.migratsiya-oldi`.

---

## 5. Mikrofonni haqiqiy ovoz bilan sinash

Siz ~34 ta gapni o'yinda mikrofon orqali aytdingiz:

**To'g'ri tanilganlar:**
- "Pomidor necha pul"
- "Qimmat ekan arzonroq bering"
- "O'n ming qilib bering"
- "Ikki kilo"
- "Bir kilo tortib bering"
- "Kechirasiz men bir kilo so'ragan edim"
- "Iltimos ozgina arzonroq qilib bering"
- "Assalomulaykum"
- "Kerak emas rahmat"

**Xatolar va ularning tuzatilishi:**
| Muammo | Sabab | Tuzatish |
|---|---|---|
| Birinchi gap buzilardi ("Sirimli kun") | Mikrofon V bosilganda endi ochilardi, gap boshi yo'qolardi | Mikrofon suhbat boshida ochiladi, oldingi 0.5 s ham yoziladi |
| Ovoz juda past | Stereo kanallar o'rtachalanardi, kuchaytirish x8 bilan cheklangan edi | Eng baland kanal olinadi, kuchaytirish x30 gacha |
| Ovoz buzilishi | Filtrsiz resemplash (aliasing) | Filtrli resemplash |
| Oxirgi bo'g'in kesilardi | Tugma qo'yilgan zahoti yozish to'xtardi | +0.35 s davom etadi |

Tavsiya: Windows'da mikrofon darajasini 80–100% ga ko'taring. Hozir xom signal cho'qqisi atigi 0.05–0.1.

---

## 6. Litsenziyalar

| Komponent | Litsenziya | Talab |
|---|---|---|
| CrispASR | MIT | Litsenziya fayli o'yin bilan birga beriladi |
| NVIDIA FastConformer uz | CC-BY-4.0 | O'yinda "NVIDIA NeMo" deb ko'rsatish kerak |
| llama.cpp | MIT | — |
| alloma (Llama 3.2/3.1) | Llama litsenziyasi | "Built with Llama" deb yozish kerak |
| Navoiy TTS, rubaiSTT | Apache-2.0 | — |
| Piper | MIT (espeak-ng **GPL**) | espeak ishlatilmaydi |

---

## 7. Keyingi qadamlar

1. **Sotuvchi ovozi (TTS):** Navoiy yordamida sotuvchilar ovozi tayyorlanadi, so'ng kichik Piper/VITS ovozi o'qitiladi. Tayyor gaplar oldindan WAV qilib qo'yiladi.
2. **O'qitilgan miya:** bozor suhbatlari to'plami tuziladi va 0.6–1B model LoRA bilan o'qitiladi. Natijada u kutilmagan gaplarga ham javob bera oladi.
3. **STT:** bola ovozi va shovqinli muhitda sinash. Jurnal (`jurnal.tsv`) asosida xatolar tahlil qilinadi.
4. **NPC migratsiyasi:** Citizen tuzilishiga ko'chirish ishini (zaxiradagi kod) xarita bilan birga oxiriga yetkazish.

---

## 8. Fayllar

| Fayl | Nima |
|---|---|
| `Source/BozorUnreal/BozorQuloq.*` | Mikrofon va STT |
| `Source/BozorUnreal/BozorSotuv.cpp` | Erkin gapni tushunish |
| `Source/BozorUnreal/BozorSotuvTest.cpp` | Avtomatik sinovlar |
| `AI/poc/` | 0-bosqich o'lchov skriptlari va `NATIJA.md` |
| `AI/stt/yukla.sh` | STT fayllarini yuklash |
| `Saved/Quloq/jurnal.tsv` | Mikrofon yozuvlari jurnali (lokal) |
