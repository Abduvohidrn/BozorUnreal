# Senariy: "Mehmonlar keldi" — onaning qo'ng'irog'i va mehmon uchun bozorlik

O'yinchi — 10–13 yoshli bola. U avtobusda bozorga keladi. Bekatda tushganda onasi telefon qilib, uyga mehmon
kelganini aytadi va ro'yxatdagi narsalarni tezroq olib kelishni so'raydi. Bola o'sha ro'yxat bilan bozorlik qiladi.

Kod: `Source/BozorUnreal/BozorTelefon.*` (qo'ng'iroq), `BozorHud.*` (telefon → ro'yxat qog'ozi),
`BozorSotuv.*` (daraja ma'lumoti). Ma'lumot: `Content/Bozor/Data/sotuv.json` → `darajalar[]`.
Telefon ko'rinishi va tovushlari — Advanced Phone System asseti (`Content/PhoneSystem`).

## Sahna ketma-ketligi

| # | Sahna | Nima bo'ladi | Bola nima qiladi |
|---|-------|--------------|------------------|
| 1 | Avtobus | Avtobus bekatga keladi, eshik ochiladi, bola yo'lakka tushadi, avtobus ketadi | Bo'sh joy / Enter — introni o'tkazib yuborish |
| 2 | **Qo'ng'iroq** | Telefon ekranning o'ng tomonidan chiqib titraydi, jiringlaydi: "Kiruvchi qo'ng'iroq — Oyijon" | **E** (yoki Enter) — javob berish. 15 s javob berilmasa o'zi ulanadi |
| 3 | Suhbat | Telefonda vaqt sanagichi yuradi, ekran pastida subtitr chiqadi | E — keyingi gap (o'zi ham o'tadi) |
| 4 | Qo'ng'iroq tugaydi | Telefon cho'ntakka tushadi | — |
| 5 | Ro'yxat | Xarid ro'yxati qog'ozi cho'ntakdan chiqadi (mollar, pul, onaning eslatmasi) | Yura boshlasa yopiladi, keyin Tab bosib turib ko'riladi |
| 6 | Bozorlik | Har bir mol uchun xarid bosqichlari — `Senariy_Tarozi.md` | Salom, savdolashish, tarozi, to'lov, qaytim |
| 7 | Qaytish | Ro'yxat tugagach chiqishga qaytiladi, natija oynasi chiqadi | Enter — keyingi daraja / qaytadan |

Telefon suhbati davomida E tugmasi faqat telefonga tegishli: rasta yonida bosilsa ham sotuvchi bilan suhbat ochilmaydi.

## Qo'ng'iroq matni (4-daraja)

> **Oyijon:** Allo, bolam! Bozorga yetib keldingmi?
> **Bola:** Ha, oyijon, hozir avtobusdan tushdim.
> **Oyijon:** Voy bolam, mehmonlar kelib qolishdi! Xolang oilasi bilan kelibdi.
> **Oyijon:** Mehmonlarga osh damlayman, achchiq-chuchuk qilaman, dasturxonga meva ham kerak.
> **Oyijon:** Cho'ntagingdagi ro'yxatga qara - hammasini yozib qo'yganman. Go'shtni dadang olib keladi.
> **Oyijon:** Ro'yxatdagi narsalarni tezroq olib kel. Qaytimni sanab ol, xo'pmi?
> **Bola:** Xo'p, oyijon! Hammasini olib, tez boraman.
> **Oyijon:** Barakalla, bolam. Ehtiyot bo'l, kutyapmiz!

Navoiy TTS ishlab tursa, onaning gaplari ayol ovozida ("mehribon" uslubi) o'qiladi, bozor shovqini pasayadi
(`SC_Ovoz` ducking). Ishlamasa faqat subtitr chiqadi. Bola gaplari — faqat subtitr.

## Mehmon uchun ro'yxat (4-daraja)

O'zbek oilasida mehmon kelsa: **osh**, **achchiq-chuchuk** va salat, dasturxonga **non va meva**, choyga **limon**.
Ro'yxatga faqat bozorda haqiqatan sotiladigan mollar kiritilgan. Bozorda go'sht sotuvchisi yo'q, shuning uchun
go'shtni "dadasi olib keladi".

| Nima uchun | Mol | Miqdor |
|------------|-----|--------|
| Dasturxon | Patir | 4 dona |
| Osh | Sabzi | 3 kilo |
| Osh, achchiq-chuchuk | Piyoz | 2 kilo |
| Osh | Sarimsoq | 0,5 kilo |
| Osh | Zira | 1 qadoq |
| Achchiq-chuchuk | Qora murch | 1 qadoq |
| Achchiq-chuchuk | Pomidor | 2 kilo |
| Salat | Bodring | 1 kilo |
| Salat, osh ustiga | Ko'kat | 3 bog' |
| Meva | Uzum | 1 kilo |
| Meva | Anor | 1 kilo |
| Choy | Limon | 0,5 kilo |

- **Pul:** 259 000 so'm — 3 × 50 000, 3 × 20 000, 3 × 10 000, 2 × 5 000, 2 × 2 000, 4 × 1 000, 2 × 500.
  Eng qimmat narxlarda ham ~230 000 so'm ketadi — savdolashmasa ham yetadi, savdolashsa tejaydi.
- **Eslatma (qog'oz pastida):** "Mehmonlar kutyapti, tezroq kel! — Oying".
- **Qaytim:** sotuvchilar ba'zan kam beradi (`xatoQaytim: true`).
- **Tarozi rejasi:** `["togri", "ortiq", "togri", "kam", "togri", "ortiq"]` — 2-tortish ko'p, 4-tortish kam,
  6-tortish yana ko'p; qolganlari to'g'ri.
- Bitta sotuvchidan bir nechta mol olinadi (masalan, Sardor aka: sabzi, pomidor, bodring; ziravorchi: zira va murch).

## Darajalar

| Daraja | Nomi | Qo'ng'iroq | Ro'yxat |
|--------|------|------------|---------|
| 1 | Birinchi xarid | **Bor** — ona bitta non olib kelishni, salom berishni va qaytimni sanashni eslatadi | Non — 1 dona |
| 2 | Ro'yxat | Yo'q | Non, kartoshka, olma |
| 3 | Pul kam | Yo'q | Non, kartoshka, olma (pul kam, qaytim xatosi) |
| 4 | **Mehmonlar keldi** | **Bor** — yuqoridagi suhbat | 12 ta mol (yuqorida) |

1-daraja qo'ng'irog'i:

> **Oyijon:** Allo, bolam! Bozorga yetib keldingmi?
> **Bola:** Ha, oyijon, hozir avtobusdan tushdim.
> **Oyijon:** Barakalla! Ro'yxatga qara - bitta non olib kel, xo'pmi?
> **Oyijon:** Sotuvchiga avval salom ber, qaytimni ham sanab ol.
> **Bola:** Xo'p, oyijon! Hozir olib boraman.
> **Oyijon:** Yaxshi, bolam. Ehtiyot bo'l!

Qo'ng'iroqni boshqa darajaga qo'shish uchun `sotuv.json` dagi darajaga `qongiroq` qo'shiladi:

```json
"qongiroq": {
  "kim": "Oyijon",
  "gaplar": [
    ["ona", "Allo, bolam!"],
    ["bola", "Ha, oyijon."]
  ]
}
```

`"bola"` — bola javobi, boshqa har qanday kalit — qo'ng'iroq qilgan odam gapi.

## Natija ekrani

Uzun ro'yxatda qog'oz va natija jadvali ekranga sig'ishi uchun qatorlar va matn birga kichrayadi.
Yulduzlar odatdagidek: 1 — ro'yxat to'liq, +1 — odob, +1 — qaytim va tarozi xatolari sezilgan va savdolashib tejalgan.

## Ishga tushirish va sinovlar

```bash
Bozor_Oyin.bat 4
```

| Buyruq qatori | Nima qiladi |
|---------------|-------------|
| `-Daraja=4` | Mehmonlar darajasi |
| `-QongiroqYoq` | Qo'ng'iroqsiz o'ynash |
| `-QongiroqTest` | Qo'ng'iroq o'zi ulanadi, suratlar (`Saved/Screenshots/QongiroqTest`: jiringlash, suhbat, ro'yxat), keyin o'yin yopiladi |
| `powershell -File Scripts\savdo_sinov.ps1 -Daraja 4` | Bot 12 molni o'zi sotib oladi; YASHIL bo'lishi kerak |

Avto-sinovlarda (`-DarajaTest`, `-SotuvTest`, `-BozorAutoTest`, `-BozorQadamTest`, `-AvtobusTest`) qo'ng'iroq chiqmaydi.
