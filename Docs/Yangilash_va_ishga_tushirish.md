# Loyihani yangilash va ishga tushirish (Claude uchun prompt)

Yangi yoki boshqa kompyuterda Claude Code'ga quyidagi matnni bering.

```
Menga Unreal Engine loyihasini GitHub'dan yangilab (pull), ishga tushirishda yordam ber.

Repo: https://github.com/Abduvohidrn/BozorUnreal (private, main branch)
Loyiha: BozorUnreal.uproject, Unreal Engine 5.8, C++ (modul: BozorUnreal)
Katta fayllar (uasset, umap, rasm, ovoz) Git LFS'da.

Qadamlar:
1. Git va Git LFS borligini tekshir (`git --version`, `git lfs version`).
   Git LFS yo'q bo'lsa, o'rnatishni mendan so'ra, keyin `git lfs install` ni ishga tushir.
2. Loyiha kompyuterda bormi, tekshir (`BozorUnreal.uproject` ni qidir; asosiy joyi E:\TRK\BozorUnreal).
   - Yo'q bo'lsa: kerakli papkaga clone qil:
     git clone https://github.com/Abduvohidrn/BozorUnreal.git "<papka>\BozorUnreal"
   - Bor bo'lsa: avval `git status` ni ko'r. Commit qilinmagan o'zgarishlar bo'lsa, menga
     ro'yxatini ko'rsat va so'ramasdan o'chirma, ustidan yozma, stash ham qilma.
     Toza bo'lsa: `git pull`, keyin `git lfs pull`.
   - Unreal muharriri shu loyiha bilan ochiq bo'lsa, pull'dan oldin saqlab yopishimni so'ra.
   GitHub login so'ralsa, kirishni men o'zim qilaman.
3. LFS fayllari to'liq tushganini tekshir: `git lfs ls-files` soni va .uasset fayllar
   kichik "pointer" matn fayl bo'lib qolmaganini (200 baytdan kichik .uasset bo'lmasin).
4. Dvigatelni toping: .uproject dagi "EngineAssociation" GUID'ni reestrdan
   (HKCU\Software\Epic Games\Unreal Engine\Builds) qidirib, shu yo'ldagi UE 5.8 ni ishlat.
   Boshqa UE 5.8 bilan build qilma - PCH to'qnashuvi (C2953 SelectIntPointerType) beradi.
   Visual Studio C++ build vositalari borligini tekshir.
5. Muharrir yopiq holda C++ ni build qil:
   "<UE>\Engine\Build\BatchFiles\Build.bat" BozorUnrealEditor Win64 Development -Project="<loyiha>\BozorUnreal.uproject" -WaitMutex -NoHotReload
   Xato chiqsa, sababini top va menga tushuntir.
6. Tekshiruv uchun savdo sinovini ishga tushir (bot o'zi o'ynab, o'zi yopiladi):
   powershell -File Scripts\savdo_sinov.ps1 -UE "<UE>"
   Natija oxirida "YASHIL" bo'lishi kerak. "QIZIL" bo'lsa, qaysi tekshiruv yiqilganini ayt.
7. Hammasi ishlasa, o'zim o'ynashim uchun shu buyruqni ber:
   "<UE>\Engine\Binaries\Win64\UnrealEditor.exe" "<loyiha>\BozorUnreal.uproject" /Game/Bozor/Maps/Bozor -game -windowed -ResX=1600 -ResY=900

Loyiha haqida qisqacha:
- Bozor o'yini: 10-13 yoshli bola xaridor, sotuvchilar bilan tanlov tugmalari va ovoz orqali gaplashadi.
- Sotuv mantig'i: Source/BozorUnreal/BozorSotuv.*, o'yinchi va mol uzatish: BozorOyinchi.*,
  sotuvchi animatsiyalari: BozorNpc.*. Ma'lumot: Content/Bozor/Data/sotuv.json.
- Header (.h) o'zgarsa, muharrir yopiq holda Build.bat bilan build qilinadi.
- Har bir ishdan oldin /find-skills bilan mos skill topib ishla; begona skillni o'rnatishdan
  oldin fayllarini o'qib, menga ko'rsatib, ruxsat so'ra (-y ishlatma).
- Menga "Akajon" deb murojaat qil, hech qachon "Ukajon" dema. O'zbek tilida javob ber.
```
