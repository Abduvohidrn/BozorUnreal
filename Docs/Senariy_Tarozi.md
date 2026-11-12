# Senariy: "Tarozi" — savdolashish, tortish, aytish yoki aytmaslik

O'yinchi — 10–13 yoshli bola. Onasi cho'ntagiga ro'yxat va pul solib bozorga yuborgan.
Har bir xarid quyidagi bosqichlardan o'tadi. Kod: `Source/BozorUnreal/BozorSotuv.*`, ma'lumot: `Content/Bozor/Data/sotuv.json`.

## Bitta xarid bosqichlari

| # | Bosqich | Bola tanlovlari | Ta'siri |
|---|---------|-----------------|---------|
| 1 | Kutib olish | "Assalomu alaykum!" / "Pomidor necha pul?" / "Hoy, bu nechpul?" | Salomsiz boshlansa — sotuvchi eslatadi, munosabat −10 |
| 2 | Narx | "Iltimos, arzonroq qilib bering" / "Qimmat-ku!" / "Olaman, tortib bering" | Odobli so'rov narxni tushiradi, qo'pol so'rov tushirmaydi (qattiq sotuvchi narxni ko'taradi) |
| 3 | Miqdor | "+ / −", "Iltimos, 2 kilo bering" | Sotuvchi tortadi |
| 4 | **Tarozi** | "Mayli, rahmat" / "Kechirasiz, men 2 kilo so'ragandim" / "Hoy, tarozingiz noto'g'ri-ku!" | Pastda batafsil |
| 5 | To'lov | Kupyuralarni peshtaxtaga qo'yish, "Mana, X so'm" | Pul yetmasa — kamroq olish yoki yana savdolashish |
| 6 | Qaytim | "Rahmat!" / "Kechirasiz, qaytimni kam berdingiz" | 3-darajada sotuvchi ba'zan kam beradi |
| 7 | Xayrlashish | "Rahmat, xayr!" / "Yana boshqa narsa olaman" | |

## Tarozi bosqichi

Sotuvchi tortib, summani aytadi. Bola tarozini (dona mollarda xaltani) o'zi ko'radi:
`Mana, 2,5 kilo kartoshka. Jami 15 000 so'm bo'ldi. [Tarozida: 2,5 kilo]`

| Tarozida | Bola indamasa | Bola odob bilan aytsa | Bola qo'pol aytsa |
|----------|---------------|------------------------|-------------------|
| To'g'ri | Pul bosqichiga o'tadi | "To'ppa-to'g'ri tortdim" — munosabat −5 | Munosabat −5 va qo'pollik hisobi |
| **Ko'p** (bir qadam ortiq) | Ko'proq oladi, ko'proq to'laydi; *tarozi sezilmadi* +1 | Sotuvchi uzr so'rab ortig'ini oladi; *tarozi sezildi* +1 | Xato tuzatiladi, lekin munosabat tushadi |
| **Kam** (kilo 80%, dona −1) | Kam oladi, so'ralgani uchun to'laydi; *tarozi sezilmadi* +1 | Sotuvchi qo'shib beradi; *tarozi sezildi* +1 | Xato tuzatiladi, lekin munosabat tushadi |

Tarozi sezilmasa 3-yulduz berilmaydi (qaytim xatosi bilan bir xil).

## Darajalar bo'yicha reja (`sotuv.json` → `darajalar[].tarozi`)

1. **Birinchi xarid** — `["ortiq"]`. 1 ta non so'raladi, sotuvchi 2 ta soladi. Bola indamasa, 2 ta non narxi 9 000 so'mlik puldan oshib ketishi mumkin. Bu "aytmaslik"ning narxini birinchi darajadayoq ko'rsatadi.
2. **Ro'yxat** — `["togri", "ortiq", "kam"]`. Non to'g'ri, kartoshka 2,5 kilo (ko'p), olma 0,8 kilo (kam, lekin 1 kilo puli so'raladi).
3. **Pul kam** — `["kam", "ortiq", "togri"]`, qaytim xatosi bilan birga.

## Natija ekrani

- Yulduzlar, sarflangan va qolgan pul, tejalgan pul, odob, qaytim, **tarozi** (aytildi / indamadi).
- **Xaridlar jadvali** — har bir mol uchun: kerak edi, olindi (+/− farq bilan), to'landi, narx (so'ralgan → kelishilgan) va baho:
  *to'g'ri* (yashil), *ko'p* (to'q sariq), *kam* / *olinmadi* (qizil), *ro'yxatda yo'q* (kulrang).
- Eng muhim maslahat (masalan, "Tarozini kuzating: ko'p yoki kam tortilsa, odob bilan ayting.").
