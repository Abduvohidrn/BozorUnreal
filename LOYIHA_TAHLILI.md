# BozorUnreal — loyiha tahlili

O‘rganilgan sana: 2026-10-30. Kod, konfiguratsiya, asset kataloglari, Unity eksporti va mavjud loglar asosida.
Bu tekshiruvda o‘yin ishga tushirilmadi, yangi build yoki cook bajarilmadi. Blueprint graf va barcha binar assetlarning ichki holati tekshirilmagan.
Ochiq Unreal Editor holati MCP orqali faqat o‘qish bilan tekshirildi: Bozor xaritasida 267 aktor, 100 NPC va 100 MetaHuman child komponent mavjud.
Joriy menejer: 57 navbat, 6 o‘rindiq, 73 animatsiya harakati, 44 yurish va 4 darvoza nuqtasi. Fon tayyor bozor_fon_mix_YANGI SoundWave’iga ulangan.
Joriy rollar: 36 sotuvchi, 28 xaridor, 15 hamroh, 10 yo‘lovchi, 2 hammol, 9 suhbatdosh. Alohida to‘rtta qassob bu jonli inventarda yo‘q.
BozorTools MCP ro‘yxatdan o‘tgan va Python so‘rovi ishladi; eski -game logidagi API xatolari joriy editor ulanishini to‘sayotgani kuzatilmadi.

## Maqsad va kirish nuqtalari

Unity’dan Unreal Engine 5.8 ga ko‘chirilgan dehqon bozori simulyatsiyasi. Bitta BozorUnreal runtime moduli, C++20.
Asosiy xarita: /Game/Bozor/Maps/Bozor.
GameMode: /Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.
O‘yinchi boshqaruvi ThirdPerson Blueprint va Enhanced Input orqali keladi; maxsus C++ player yoki GameMode mavjud emas.
Bozor_Muharrir.bat muharrirni, Bozor_Oyin.bat esa UnrealEditor.exe -game rejimini ochadi. Ikkinchisi mustaqil paketlangan o‘yin emas.

## Asosiy tizimlar

| Tizim | Fayl | Vazifa |
|---|---|---|
| NPC | Source/BozorUnreal/BozorNpc.h va .cpp | Rollar, savdo, navbat, yurish, hamroh, o‘tirish, arava va qo‘l IK |
| Fon | Source/BozorUnreal/BozorFon.h va .cpp | 7 qatlamli zonalar yoki tayyor mix, masofa, low-pass va pavilyon ta’siri |
| Ovoz | Source/BozorUnreal/BozorOvoz.h va .cpp | Sotuvchi gaplari, rasta SFX, o‘yinchi qadamlarini aniqlash |
| Muharrir yordamchilari | Source/BozorUnreal/BozorLevelTools.h va .cpp | ISM guruhlar, NavBlocker, collision, navigatsiya, skelet va komponent vositalari |
| Import | Scripts/import_assets.py | Tekstura, material, mesh, personaj va animatsiya importi |
| Sahna | Scripts/build_level.py | Unity scene.json asosida xaritani qayta yaratish |
| NPC joylash | Scripts/bozor_npc.py | MetaHuman, kiyim, rollar, animatsiyalar va menejer ma’lumotlari |
| Go‘sht pavilyoni | Scripts/bozor_gosht.py | Peshtaxta, go‘sht, qassob va qo‘shimcha navbatlar |

## NPC xulqi

ABozorNpc — ACharacter, kontrolleri ADetourCrowdAIController. Xulq StateTree yoki paketlarning AI grafidan emas, C++20 korutinlardan boshqariladi.
FBozorCo ichki korutinlar va kutishlarni har Tick davom ettiradi. FBozorWait{0} keyingi kadrni kutadi.

- Sotuvchi: idle/busy/lean, yo‘lovchiga chaqiriq, xaridorga salom, savdolashish, tortish, uzatish va rahmat.
- Xaridor: yaqinlik, navbat uzunligi va rasta mashhurligi asosida navbat tanlaydi; sabri tugasa ketadi.
- Hamroh: yetakchining yonidan ergashadi, navbatda unga xalal bermaydigan joyda kutadi.
- Yo‘lovchi: yuradi, ba’zan to‘xtaydi yoki bo‘sh o‘rindiqqa o‘tiradi.
- Hammol: yurish sikli va maxsus arava boshqaruvi; kapsula arava o‘qini ifodalaydi, tana dastaning ortida joylanadi.
- Suhbatdosh: guruhdagi birinchi a’zo gapirish navbatini boshqaradi, qolganlar tinglaydi.

ABozorNpcManager animatsiya kategoriyalari, navbatlar, o‘rindiqlar, yurish/darvoza nuqtalari va savdo statistikalarini saqlaydi.
Xaridor rastada ko‘rib chiqqach 80% ehtimol bilan savdolashadi, undan keyin 75% ehtimol bilan xarid qiladi. Bu NPC dekorativ simulyatsiyasi; tekshirilgan C++ kodda o‘yinchi inventari, pul balansi yoki mahsulot zaxirasi tizimi yo‘q.
AnimBP yurish/tik turishni, DefaultSlot’dagi dinamik montajlar qolgan harakatlarni beradi.

## MetaHuman

bozor_npc.py da USE_MH=True. 13 eski personaj nomi MetaHuman nomlariga moslangan.
Tana MetaAnim/ABP_Manny orqali animatsiya oladi; MetaHuman Blueprint child actor sifatida yuz, groom va LODSync’ni olib keladi.
Kiyim tanaga leader pose orqali ergashadi. Fab kiyim to‘plami rolga qarab tanlanadi, topilmasa eski moslashtirilgan kiyim ishlatiladi.
MH to‘plami to‘liq topilmasa eski personajga qaytish mavjud, ammo asosiy ABP yuklashida null tekshiruvi yo‘q.
Go‘sht pavilyonidagi qassoblarni yaratadigan alohida yo‘l hozir eski Characters mesh va Pack/Queue AnimBP’dan foydalanadi.

## Ovoz

Sotuvchiga jins/yurish uslubiga qarab persona ajratiladi. Gap bo‘limi uning koordinatasidan aniqlanadi.
Bir manba yozuvi RepeatWindow davomida takrorlanmaydi; gaplar masofa, cooldown va concurrency bilan cheklanadi.
Savdo bosqichlari tarozi, paket, pul va tanga ovozlarini chaqiradi. Rasta nuqtalari yaqin tinglovchi uchun tasodifiy SFX beradi.
Qadamlar oyoq suyaklarining vertikal harakatidan aniqlanadi; sirt complex trace va PhysicalMaterial’dan olinadi, suyak bo‘lmasa masofa bo‘yicha fallback bor.
SoundClass daraxti: Master → Fon, Ovoz, SFX, UI, Musiqa. Sotuvchi gapi fonni pasaytiruvchi passiv mix’ga ega.
Asosiy fon uchun tayyor mix topilsa 7 qatlamli MetaSound yo‘li ishlatilmaydi; kirish va ovqat zonalarining qo‘shimcha AmbientSound’lari mavjud.

## Import va sahna qurish

Tashqi manba: D:/Trk/BozorKit_hammasi/Unreal_Export/scene.json. Tekshiruv vaqtida mavjud; unda 3449 sahna obyekti va 100 bazaviy NPC bor.
Unity metr va Y-up koordinatasi Unreal sm va Z-up’ga (x,y,z) → (z,x,y)*100 orqali o‘tkaziladi.
Import materiallari M_BozorLit asosida; normal green channel, sRGB/linear, smoothness → roughness va UV offset konversiyalari mavjud.
Obyektlar mesh/material/shadow bo‘yicha InstancedStaticMesh komponentlariga birlashtiriladi.
DX12/SM6, Lumen, Virtual Shadow Maps yoqilgan, static lighting o‘chiq. NavMesh dynamic, agent radius 30 sm, CrowdManager MaxAgents=200.

Asosiy ketma-ketlik: asset importi → kerakli retarget → build_level → NPC/ovoz/go‘sht joylash → navigatsiya → xaritani saqlash.
build_level.py mavjud xaritani o‘chirib qayta yaratadi. Qo‘lda qilingan sahna o‘zgarishlarini yo‘qotishi mumkin.
rebuild_npcs.py barcha ABozorNpc va menejerni o‘chiradi, bazaviy 100 NPC’ni tiklaydi; qassoblar uchun keyin gosht_yangila.py kerak.
Qayta joylashdan keyin ovoz nuqtalarini ham yangi aktorlar bilan mosligini tekshirish kerak.

## Asset inventari

Content bo‘yicha 4278 .uasset va 12 .umap topildi. Bularning hammasi asosiy xaritada ishlatilishi tasdiqlanmagan.
Content/Bozor ichida: Meshes 380, Materials 358, Textures 452, Characters 14, Animations 60, MetaAnim 65, MetaKiyim 33, Audio 235 fayl.
Textures taxminan 968 MB, MetaKiyim 284 MB. Ishlash tezligi yoki cook hajmini faqat shu raqamlardan baholab bo‘lmaydi.
Tashqi animatsiya paketlari va MetaHumans kataloglari ham mavjud.

## Mavjud dalillar va cheklovlar

- build24.log: Editor build Succeeded; engine header’dan C4996 va compiler versiyasi ogohlantirishlari bor. Hozirgi kod yangi build bilan tekshirilmagan.
- Saved/rebuild_npcs.txt: 100 MetaHuman, 36 sotuvchi, 28 xaridor, 15 hamroh, 10 yo‘lovchi, 2 hammol, 9 suhbatdosh; 57 navbat, 6 ishlatiladigan o‘rindiq, 73 harakat, topilmagan kliplar yo‘q.
- Saved/gosht_yangila.txt: 4 qassob, jami 61 navbat va 40 go‘sht bo‘lagi. Bu loglar turli ishga tushirishlarga tegishli; hozirgi xaritaning jonli inventari emas.
- 2026-10-29 BozorTest logida 78 soniyalik yakun: 100 NPC, 35 boshlangan savdo, 12 xarid. Logdagi 57 FPS bitta tarixiy o‘lchov, hozirgi MetaHuman sahnasining benchmark’i emas.
- Eski -game testida editor toolset Python API’lari topilmagan: ToolsetDefinition, AgentSkill va MetaHumanCharacterEditorSubsystem. Bugungi editor logida shu xatolar takrorlanishi tasdiqlanmadi.

## Keyingi ishlar uchun nozik joylar

1. Qayta qurish zanjiri: NPC rebuild qo‘shimcha qassoblarni yo‘qotadi; to‘liq tiklash tartibini saqlash zarur.
2. Ko‘chirish qulayligi: import, audio, log, Fab cache va UE yo‘llari ko‘p joyda absolyut yozilgan. MetaHuman kiyim tayyorlash skriptlari bu Scripts katalogida to‘liq mavjud emas.
3. Dinamik NPC o‘chirish: CustomerLoop/DealWith va TalkerLoop korutinlarida raw actor pointer’lar kutishlar oralig‘ida saqlanadi; EndPlay savdo/navbatni faol tozalamaydi. Hozirgi joylashtirilgan NPC sahnasidan tashqari spawn/despawn qo‘shilganda tekshirish kerak.
4. Navbat API’sining ayrim metodlari indeksni tekshirmaydi. Menejer ma’lumotlari runtime’da o‘zgarsa alohida himoya kerak.
5. Toolset init: Content/Python/init_unreal.py faqat ImportError’ni ushlaydi; editor API yo‘q bo‘lganda eski logdagi AttributeError undan chiqib ketadi.
6. MetaHuman unumdorligi: yashirin asosiy mesh AlwaysTickPoseAndRefreshBones bilan ishlaydi; child actor tana/yuz/groom va kiyim xarajatini yangi profiling bilan o‘lchash kerak.
7. Shipping holati noma’lum: Editor build va -game ishlashi mustaqil cook/package tekshiruvi o‘rnini bosmaydi.

O‘rganish davomida mavjud kod, asset va sozlamalar o‘zgartirilmadi; faqat shu hujjat qo‘shildi.
