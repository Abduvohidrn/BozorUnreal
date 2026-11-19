#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "BozorSotuv.generated.h"

class ABozorNpc;

// Sotuv jarayoni: o'yinchi sotuvchi bilan gaplashadi.
// "Miya" (UBozorSotuvchiMiya) faqat gapni tushunadi va javob matnini beradi; narx, pul va qaytimni
// suhbat (UBozorSuhbat) hisoblaydi va tekshiradi - miya keyin AI (local yoki Claude) bilan almashtiriladi.
// Mahsulotlar, narxlar va sotuvchi fe'llari: Content/Bozor/Data/sotuv.json (UBozorSotuvTizimi yuklaydi).

USTRUCT(BlueprintType)
struct FBozorMahsulot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id = TEXT("pomidor");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Nomi = TEXT("pomidor");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Birlik = TEXT("kilo");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Narx = 12000;        // so'ralgan narx, bir birlik uchun
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 EngPastNarx = 10000; // bundan arzon sotmaydi
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Qadam = 1000;        // savdolashganda bir tushirish
	// bola gapida mahsulotni tanish uchun so'zlar (kichik harfda)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FString> Sozlar;
};

// Sotuvchining fe'li: qancha tushirishi va qanday gapirishi shunga bog'liq
UENUM(BlueprintType)
enum class EBozorFel : uint8
{
	Mehribon,
	Qattiq,
	Shoshqaloq,
	Ayyor
};

// Bitta savdo rastasi: sotuvchi, uning fe'li va mollari (narxlari shu o'yin uchun tanlangan)
// Bitta sotuvchining xulq-atvori: Content/Bozor/Data/xulq.json (kalit - NPC nomi, masalan "BozorNpc_267").
// LLM'ga "kimligi" sifatida beriladi (UBozorAiMiya::Tizim); savdo sozlamalari va tayyor gaplar ham shundan ustun yoziladi.
// Matnlarda son va son so'zlari ("o'n", "ming") bo'lmasin: LLM ularni javobga qo'shsa, son tekshiruvi javobni rad etadi.
struct FBozorXulq
{
	FString Ism;              // "Malika opa"; bo'sh - NPC nomidan tasodifiy
	int32 Yosh = 0;           // 0 - NPC ko'rinishidan
	FString Tavsif;           // kimligi, hayoti (1-2 gap)
	FString Odatlar;          // qanday gapiradi: so'z odatlari, murojaati
	FString Yoqtiradi, Yoqtirmaydi;
	FString SavdoUslubi;      // qanday savdolashadi (narx tushirishni so'rashsa qanday javob qaytaradi)
	FString Kayfiyat;         // bugungi holati: "kayfiyatlar" dan har o'yinda bittasi tasodifan
	TArray<FString> Hayot;    // erkin suhbatda aytishi mumkin bo'lgan hayotiy tafsilotlar
	TArray<TPair<FString, FString>> Namunalar;   // ma'no -> shu sotuvchi qanday aytadi
	int32 BoshMunosabat = 50; // bolaga birinchi munosabati (0..100)
	// odob (o'yin odobnoma emas - har sotuvchi o'zicha): SalomTalab - bola salomsiz gap boshlasa "salom bering" deyish
	// ehtimoli (0 - e'tibor bermaydi, munosabat ham tushmaydi); Tanbeh - qo'pol gapga tanbeh berish ehtimoli;
	// QopolgaMunosabat - qo'pol gapga qanday munosabat bildiradi (LLM'ga beriladi)
	float SalomTalab = 1.f;
	float Tanbeh = 1.f;
	FString QopolgaMunosabat;
	// o'z ovozi ("ovoz": {"ohang", "tezlik"}): Ohang - Navoiy namuna ovozini necha yarim tonga siljitish (har sotuvchiga boshqa
	// tembr); OvozTezlik - o'yinda jonli gapni chalish tezligi (1 dan past - sekinroq va pastroq: keksalar)
	float Ohang = 0.f;
	float OvozTezlik = 1.f;
	// namuna ovoz (AI/navoiy/ovozlar/<namuna>.wav, masalan "odamlar/erkak_cv_137014"); bo'sh - fe'l x jins asl ovozi
	FString OvozNamuna;
	// ofis Navoiy serveridagi ovozi ("ovoz.server": malika_kore, jj_4, erkak ...) va gapirish tempi ("ovoz.temp", 0 - fe'ldan).
	// Bo'sh - fe'l x jinsdan (UBozorNavoiy::MasofaviyOvoz). Temp - natijaviy tezlik: serverga temp/tezlik so'raladi, chunki
	// o'yin ovozni "tezlik" (pitch) bilan chaladi - erkaklar bitta server ovozida bo'lsa ham tembri farq qiladi
	FString ServerOvoz;
	float Temp = 0.f;
	bool Bormi() const { return !Tavsif.IsEmpty(); }
};

USTRUCT(BlueprintType)
struct FBozorRasta
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) TObjectPtr<ABozorNpc> Sotuvchi = nullptr;
	UPROPERTY(BlueprintReadOnly) int32 Stall = -1;
	UPROPERTY(BlueprintReadOnly) EBozorFel Fel = EBozorFel::Mehribon;
	UPROPERTY(BlueprintReadOnly) float RadEhtimoli = 0.35f;   // birinchi savdolashuvda "narxi shu" deyish ehtimoli
	UPROPERTY(BlueprintReadOnly) int32 OdobBall = 12;    // odobli gap munosabatni shuncha oshiradi
	UPROPERTY(BlueprintReadOnly) int32 QopolBall = -12;  // qo'pol gap shuncha tushiradi
	UPROPERTY(BlueprintReadOnly) float XatoQaytim = 0.f; // qaytimni kam berish ehtimoli
	UPROPERTY(BlueprintReadOnly) TArray<FBozorMahsulot> Mollar;
	UPROPERTY(BlueprintReadOnly) TArray<FVector> MolJoylari;   // har mol uyumining tepasi (narx taxtachasi uchun), Mollar tartibida
	// har molning rastadagi eng kichik mesh'i (bitta dona: non, qovun...) - o'yinchiga uzatish sahnasi uchun, Mollar tartibida
	UPROPERTY(BlueprintReadOnly) TArray<TObjectPtr<class UStaticMesh>> MolMeshlari;
	UPROPERTY(BlueprintReadOnly) FVector Markaz = FVector::ZeroVector;   // sotuvchi va navbat orasi
	UPROPERTY(BlueprintReadOnly) FVector Xaridorga = FVector::ForwardVector;   // sotuvchidan xaridor tomonga (gorizontal)

	// shu sotuvchining o'z xulq-atvori (xulq.json; bo'lmasa bo'sh - faqat fe'l)
	FBozorXulq Xulq;

	// fe'lga xos gaplar: javob turi nomi (EBozorJavob) -> variantlar (xulq.json "gaplar" ustidan yoziladi)
	TMap<FString, TArray<FString>> Gaplar;
	// sotuv.json "gaplar" (hamma sotuvchiga umumiy; UBozorSotuvTizimi'niki) va ovoz uslubi (Navoiy: mehribon, xotirjam ...)
	const TMap<FString, TArray<FString>>* Asosiy = nullptr;
	FString Uslub = TEXT("xotirjam");
};

UENUM(BlueprintType)
enum class EBozorBosqich : uint8
{
	Boshlanish,
	NarxAytildi,
	PulKutilmoqda,
	QaytimTekshir,   // pul olindi, qaytim berildi - bola sanab, rozi bo'ladi yoki "kam" deydi
	Tugadi,
	TaroziTekshir    // tortildi - bola tarozini (xaltani) ko'radi: ko'p yoki kam bo'lsa aytadi yoki indamaydi, keyin pul
};

// Tarozi natijasi (daraja rejasidagi xato va bolaning tanlovi)
UENUM(BlueprintType)
enum class EBozorTarozi : uint8
{
	Togri,
	OrtiqAytdi,   // ko'p tortildi - bola aytdi, sotuvchi ortig'ini oldi
	OrtiqOldi,    // ko'p tortildi - bola indamadi, ko'proq oldi va ko'proq to'ladi
	KamAytdi,     // kam tortildi - bola aytdi, sotuvchi qo'shib berdi
	KamOtkazdi    // kam tortildi - bola sezmadi, kam oldi, to'liq to'ladi
};

// Miya so'raydigan harakat - suhbat uni tekshirib bajaradi
UENUM(BlueprintType)
enum class EBozorHarakat : uint8
{
	Yoq,
	Salom,
	NarxAytish,
	Savdolashish,
	Tortish,       // Qiymat - miqdor (birlik)
	PulQabul,      // Qiymat - o'yinchi bergan pul
	Xayr,
	Tushunmadi,
	MolTanlash,    // Mol - qaysi mahsulot (boshqa harakatsiz faqat nomini aytdi)
	QaytimRozi,    // qaytimni oldi, rahmat
	QaytimKam,     // "qaytim kam berdingiz"
	TaroziRozi,    // tarozidagini oladi (xato bo'lsa ham indamaydi)
	TaroziAyt      // "men X so'ragandim" - ko'p yoki kam tortilganini aytadi
};

USTRUCT(BlueprintType)
struct FBozorSuhbatHolati
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) TArray<FBozorMahsulot> Mollar;   // shu rastadagi hamma mol
	UPROPERTY(BlueprintReadOnly) int32 Tanlangan = INDEX_NONE;    // hozir qaysi mol haqida gap ketyapti
	UPROPERTY(BlueprintReadOnly) FBozorMahsulot Mahsulot;         // tanlangan mol (Mollar[Tanlangan] nusxasi)
	UPROPERTY(BlueprintReadOnly) EBozorFel Fel = EBozorFel::Mehribon;
	UPROPERTY(BlueprintReadOnly) float RadEhtimoli = 0.35f;
	UPROPERTY(BlueprintReadOnly) int32 OdobBall = 12;
	UPROPERTY(BlueprintReadOnly) int32 QopolBall = -12;
	// sotuvchining bolaga munosabati 0..100 (boshida 50): odobli gap oshiradi, qo'pol tushiradi.
	// 70+ - ko'proq tushiradi va iliq gapiradi, 30 dan past - tushirmaydi, 10 dan past - savdoni to'xtatadi
	UPROPERTY(BlueprintReadOnly) int32 Munosabat = 50;
	UPROPERTY(BlueprintReadOnly) float XatoQaytim = 0.f;
	UPROPERTY(BlueprintReadOnly) TArray<int32> QaytimKupyuralar;   // oxirgi berilgan qaytim
	UPROPERTY(BlueprintReadOnly) int32 QaytimKam = 0;              // qaytim shuncha kam berildi (0 - to'g'ri)
	UPROPERTY(BlueprintReadOnly) EBozorBosqich Bosqich = EBozorBosqich::Boshlanish;
	UPROPERTY(BlueprintReadOnly) int32 JoriyNarx = 0;   // hozir kelishilgan narx (bir birlik)
	UPROPERTY(BlueprintReadOnly) float Miqdor = 0.f;          // tarozida (bola aslida oladigani)
	UPROPERTY(BlueprintReadOnly) float SoralganMiqdor = 0.f;  // bola so'ragani
	UPROPERTY(BlueprintReadOnly) float HisobMiqdor = 0.f;     // sotuvchi aytgan va puli olinadigani (kam tortsa - so'ralgani)
	UPROPERTY(BlueprintReadOnly) EBozorTarozi Tarozi = EBozorTarozi::Togri;
	UPROPERTY(BlueprintReadOnly) int32 Summa = 0;
	UPROPERTY(BlueprintReadOnly) int32 Savdolashuv = 0;  // necha marta tushirish so'raldi
	UPROPERTY(BlueprintReadOnly) bool bSalomlashdi = false;
};

USTRUCT(BlueprintType)
struct FBozorTushuncha
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EBozorHarakat Harakat = EBozorHarakat::Tushunmadi;
	UPROPERTY(BlueprintReadOnly) float Qiymat = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bSalomBor = false;
	UPROPERTY(BlueprintReadOnly) int32 Mol = INDEX_NONE;   // gapda tilga olingan mahsulot (Holat.Mollar indeksi)
	UPROPERTY(BlueprintReadOnly) int32 Odob = 0;           // +1 odobli, 0 oddiy, -1 qo'pol
};

UENUM(BlueprintType)
enum class EBozorTanlovTur : uint8
{
	Gap,         // sotuvchiga aytiladi (Niyat)
	MiqdorOch,   // miqdor tanlash rejimi
	MiqdorKam,
	MiqdorKop,
	MolRoyxati,  // rastadagi boshqa mollar
	Ortga,
	TolovOch,       // pul berish: kupyuralarni peshtaxtaga qo'yish rejimi
	KupyuraQoy,     // Kupyura - qaysi kupyura
	KupyuraQaytar   // peshtaxtadagilarni qaytarib olish
};

// Bolaning bitta javob varianti (tugma)
USTRUCT(BlueprintType)
struct FBozorTanlov
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Matn;
	UPROPERTY(BlueprintReadOnly) EBozorTanlovTur Tur = EBozorTanlovTur::Gap;
	UPROPERTY(BlueprintReadOnly) FBozorTushuncha Niyat;
	UPROPERTY(BlueprintReadOnly) int32 Kupyura = 0;   // KupyuraQoy uchun
	UPROPERTY(BlueprintReadOnly) int32 Soni = 0;      // hamyonda shu kupyuradan yana nechta qoldi
};

// Suhbat natijasi - miya yozadigan javob turi. Nomlari sotuv.json "gaplar" kalitlari bilan bir xil
UENUM(BlueprintType)
enum class EBozorJavob : uint8
{
	Salom,
	Narx,
	TushirdiMan,
	TushirmaymanEngPast,
	TushirmaymanHozircha,
	MiqdorSora,
	Tortdim,
	PulYetmaydi,
	HamyondaYoq,
	PulOldim,
	Xayr,
	XayrSotibOlmay,
	SalomBering,
	Tushunmadim,
	Tugagan,
	KutibOlish,   // bola E bosganda sotuvchining birinchi gapi
	MolSora,      // rastada bir nechta mol bor, qaysi biri kerakligi noma'lum
	MolTanlandi,  // bola molni tanladi (narxini hali so'ramadi)
	QopolgaRad,   // qo'pol so'ralgani uchun tushirmaydi
	NarxniKotardi,// qo'pollikka javoban narxni ko'tardi
	Haydadi,      // munosabat juda yomon - savdoni to'xtatdi
	QaytimUzr,    // bola kam qaytimni sezdi - sotuvchi uzr so'rab qolganini beradi
	QaytimTogri,  // qaytim to'g'ri edi
	OshBolsin,    // qaytimni oldi
	TaroziUzr,    // bola ko'p/kam tortilganini aytdi - sotuvchi to'g'rilaydi
	TaroziTogri,  // tarozi to'g'ri edi
	TaroziKelishdi// bola tarozidagini oldi - sotuvchi summani aytadi
};

// Gapni tushunadi va javob matnini yozadi. Hozir - tayyor javoblar; keyin AI bilan almashtiriladi.
UCLASS(Abstract, BlueprintType)
class BOZORUNREAL_API UBozorSotuvchiMiya : public UObject
{
	GENERATED_BODY()

public:
	virtual FBozorTushuncha Tushun(const FString& Gap, const FBozorSuhbatHolati& Holat) PURE_VIRTUAL(UBozorSotuvchiMiya::Tushun, return {};);
	virtual FString Yoz(EBozorJavob Javob, const FBozorSuhbatHolati& Holat, int32 Qaytim) PURE_VIRTUAL(UBozorSotuvchiMiya::Yoz, return {};);
	// oxirgi Yoz gapining oldindan yozilgan ovoz bo'laklari (bo'lmasa bo'sh - gap faqat matn bo'lib qoladi)
	virtual TArray<FString> Bolaklar() const { return {}; }
	// qo'shimcha gap (javobdan oldin): "QopolIzoh" - bola qo'pol gapirdi, "OdobIzoh" - odobli gapirdi. Yo'q bo'lsa bo'sh
	virtual FString Izoh(const FString& Kalit) { return FString(); }
};

// Kalit so'zlar bo'yicha tushunadi, javoblarni tayyor variantlardan tasodifiy tanlaydi.
// Fe'lga xos gaplar (FBozorRasta::Gaplar) bo'lsa, avval shulardan tanlaydi, keyin sotuv.json "gaplar"dan.
// Tanlangan qolip ovoz bo'laklariga ajratiladi (AI/tts/gaplar_yoz.py ham xuddi shunday ajratib yozadi):
//   qolip matni o'rinlar orasida - "G_<md5 dan 8 belgi>", son - "S_<minglar>m" + "S_<qoldiq>" (12 500 -> S_12m, S_500),
//   miqdor - "M_<o'ndan bir>" (2,5 -> M_25), mol - "P_<id>", birlik - "B_<birlik, tutuqsiz>"; ro'yxat orasida - "va"
UCLASS()
class BOZORUNREAL_API UBozorTayyorJavob : public UBozorSotuvchiMiya
{
	GENERATED_BODY()

public:
	virtual FBozorTushuncha Tushun(const FString& Gap, const FBozorSuhbatHolati& Holat) override;
	virtual FString Yoz(EBozorJavob Javob, const FBozorSuhbatHolati& Holat, int32 Qaytim) override;
	virtual TArray<FString> Bolaklar() const override { return OxirgiBolaklar; }
	virtual FString Izoh(const FString& Kalit) override;

	TMap<FString, TArray<FString>> FelGaplari;
	const TMap<FString, TArray<FString>>* Asosiy = nullptr;

	// qolip matni bo'lagining ovoz kaliti (Python: "G_" + md5(utf8).hexdigest()[:8])
	static FString MatnKaliti(const FString& Bolak);

private:
	TArray<FString> OxirgiBolaklar;
};

// Ota-ona bergan xarid ro'yxatidagi bitta qator
USTRUCT(BlueprintType)
struct FBozorRoyxatQator
{
	GENERATED_BODY()

	FBozorRoyxatQator() = default;
	FBozorRoyxatQator(const TCHAR* N, float M, const TCHAR* B) : Nomi(N), Miqdor(M), Birlik(B) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Nomi;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Miqdor = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Birlik = TEXT("dona");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOlindi = false;
	UPROPERTY(BlueprintReadOnly) float Olindi = 0.f;   // hamma xariddan jami olingani
};

// Bitta xarid (to'langanda yoziladi) - natija statistikasi uchun
USTRUCT(BlueprintType)
struct FBozorXarid
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Nomi;
	UPROPERTY(BlueprintReadOnly) FString Birlik;
	UPROPERTY(BlueprintReadOnly) float Soralgan = 0.f;
	UPROPERTY(BlueprintReadOnly) float Olindi = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 Narx = 0;         // so'ralgan narx (bir birlik)
	UPROPERTY(BlueprintReadOnly) int32 Kelishildi = 0;   // savdolashib kelishilgan narx
	UPROPERTY(BlueprintReadOnly) int32 Tolandi = 0;
	UPROPERTY(BlueprintReadOnly) EBozorTarozi Tarozi = EBozorTarozi::Togri;
};

// Natija jadvalidagi bir qator: ro'yxatdagi (yoki ro'yxatdan tashqari olingan) mol bo'yicha jami
struct FBozorNatijaQator
{
	FString Nomi;
	FString Birlik;
	float Kerak = -1.f;   // ro'yxatda yo'q bo'lsa -1
	float Olindi = 0.f;
	int32 Tolandi = 0;
	int32 Narx = 0, Kelishildi = 0;   // birinchi xariddagi
	// "to'g'ri" / "ko'p" / "kam" / "olinmadi" / "ro'yxatda yo'q"
	FString Baho() const;
};

// Daraja: boshlang'ich pul, xarid ro'yxati, onaning eslatmasi (sotuv.json "darajalar")
USTRUCT(BlueprintType)
struct FBozorDaraja
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Raqam = 1;
	UPROPERTY(BlueprintReadOnly) FString Nomi;
	UPROPERTY(BlueprintReadOnly) FString Eslatma;
	UPROPERTY(BlueprintReadOnly) TMap<int32, int32> Kupyura;
	UPROPERTY(BlueprintReadOnly) TArray<FBozorRoyxatQator> Royxat;
	UPROPERTY(BlueprintReadOnly) bool bXatoQaytim = false;
	// tortishlar tartibida tarozi rejasi: "togri", "ortiq" (ko'p tortadi), "kam" (kam tortib, to'liq narx so'raydi)
	UPROPERTY(BlueprintReadOnly) TArray<FString> Tarozi;
	// avtobusdan tushgach keladigan telefon qo'ng'irog'i (sotuv.json "qongiroq"): kim qo'ng'iroq qilyapti va suhbat
	// (Key: true - qo'ng'iroq qilgan odam gapi, false - bola javobi). Bo'sh bo'lsa qo'ng'iroq yo'q
	UPROPERTY(BlueprintReadOnly) FString Qongiroqchi;
	TArray<TPair<bool, FString>> QongiroqGaplar;
};

// O'yinchining puli (kupyuralar), xarid ro'yxati (cho'ntakdagi qog'oz) va xarid qilganlari
UCLASS(ClassGroup = Bozor, meta = (BlueprintSpawnableComponent))
class BOZORUNREAL_API UBozorHamyon : public UActorComponent
{
	GENERATED_BODY()

public:
	UBozorHamyon();

	// kupyura qiymati -> soni (500 - tanga). Pul shulardan hisoblanadi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TMap<int32, int32> Kupyura = { {20000, 1}, {10000, 2}, {5000, 1}, {2000, 1}, {1000, 2}, {500, 2} };

	// jami pul (Kupyura yig'indisi; o'zgartirish faqat Qosh/Ol orqali)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	int32 Pul = 0;

	// qaytim tekshiruvi: sotuvchi kam qaytim berganda bola sezdimi (natija va baho uchun)
	UPROPERTY(BlueprintReadOnly, Category = "Bozor") int32 XatoQaytimSezildi = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bozor") int32 XatoQaytimOtkazildi = 0;
	// natija uchun: suhbatlar, salomsiz boshlanganlari, qo'pol gaplar, rahmat aytilgani, savdolashib tejalgan pul
	UPROPERTY(BlueprintReadOnly, Category = "Bozor") int32 Suhbatlar = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bozor") int32 Salomsiz = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bozor") int32 Qopol = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bozor") int32 Rahmatlar = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bozor") int32 Tejaldi = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bozor") int32 BoshlangichPul = 0;
	// tarozi: ko'p/kam tortilganini bola aytdimi yoki indamadimi
	UPROPERTY(BlueprintReadOnly, Category = "Bozor") int32 TaroziSezildi = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bozor") int32 TaroziOtkazildi = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Bozor") TArray<FBozorXarid> Xaridlar;
	// darajaning tarozi rejasi va nechanchi tortish
	TArray<FString> TaroziRejasi;
	int32 Tortishlar = 0;

	bool RoyxatTugadimi() const;
	// darajaning puli, ro'yxati va eslatmasi bilan boshidan boshlaydi (statistika ham nolga)
	void Boshla(const FBozorDaraja& D);
	// 1-3 yulduz: 1 - ro'yxat to'liq, +1 - odob (hamma suhbat salom bilan, qo'pol gap yo'q),
	// +1 - qaytim va tarozi xatosi o'tkazilmagan va savdolashib tejalgan
	int32 Yulduzlar() const;
	// ro'yxat qatorlari (tartibida), keyin ro'yxatdan tashqari olinganlar
	TArray<FBozorNatijaQator> NatijaQatorlari() const;

	// katta qiymatdan kichigiga: 200 000 ... 500
	static const TArray<int32>& Turlari();
	// summani eng kam sonli kupyuralarga ajratadi (sotuvchi qaytimni shunday beradi)
	static TArray<int32> Ajrat(int32 Summa);
	int32 Soni(int32 Qiymat) const { const int32* N = Kupyura.Find(Qiymat); return N ? *N : 0; }
	void Qosh(int32 Qiymat, int32 N = 1);
	bool Ol(int32 Qiymat);
	// hamyondagi kupyuralardan aynan Summa ni yig'adi (kattasidan); yig'ilmasa false
	bool AniqYigi(int32 Summa, TArray<int32>& Out) const;
	// "1 × 20 000, 2 × 10 000, ..."
	FString Tarkibi() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	TArray<FBozorRoyxatQator> Royxat = {
		{ TEXT("Non"), 2.f, TEXT("dona") },
		{ TEXT("Pomidor"), 1.f, TEXT("kg") },
		{ TEXT("Kartoshka"), 2.f, TEXT("kg") },
	};

	// qog'oz pastidagi ona yozuvi
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bozor")
	FString Eslatma = TEXT("Qaytimni sanab ol!\n— Oying");

	UPROPERTY(BlueprintReadOnly, Category = "Bozor")
	TArray<FString> Savat;
};

UCLASS(BlueprintType)
class BOZORUNREAL_API UBozorSuhbat : public UObject
{
	GENERATED_BODY()

public:
	// BoshMunosabat: sotuvchining bolaga boshlang'ich munosabati (UBozorSotuvTizimi::BoshMunosabat - xulq va xotira); -1 - xulqdan
	void Boshla(const FBozorRasta& Rasta, UBozorHamyon* Hamyon, UBozorSotuvchiMiya* Miya, int32 BoshMunosabat = -1);

	// Sotuvchining birinchi gapi (bola E bosganda) - fe'liga qarab
	FString KutibOl();

	// O'yinchi gapi -> sotuvchi javobi (matn: konsol, sinov, keyin ovoz)
	FString Yoz(const FString& Gap);

	// Tushunilgan niyat -> sotuvchi javobi (tanlov tugmalari shu yerga to'g'ridan-to'g'ri beradi)
	FString Bajar(const FBozorTushuncha& T);

	// Hozirgi holatda bolaning javob variantlari (odobli, oddiy, qo'pol; molni, miqdorni, pulni tanlash)
	TArray<FBozorTanlov> Tanlovlar() const;
	// I-variantni tanlash. Sotuvchiga gap aytilsa true (OutMen - bola gapi, OutSotuvchi - javob);
	// faqat oyna ichidagi o'tish (miqdor +/-, mollar ro'yxati) bo'lsa false
	bool Tanla(int32 I, FString& OutMen, FString& OutSotuvchi);
	bool MiqdorTanlanmoqda() const { return bMiqdorTanlash; }
	bool Tolovda() const { return bTolov; }
	float GetTanlovMiqdor() const { return TanlovMiqdor; }
	const TArray<int32>& GetPeshtaxta() const { return Peshtaxta; }

	const FBozorSuhbatHolati& GetHolat() const { return Holat; }
	ABozorNpc* GetSotuvchi() const { return Sotuvchi; }
	// oxirgi javob turi (sotuvchi harakati va ovozi shunga qarab tanlanadi)
	EBozorJavob GetOxirgiJavob() const { return OxirgiJavob; }
	// bolaning oxirgi gapi: 1 - odobli, -1 - qo'pol, 0 - oddiy (sotuvchi javobining ohangi uchun - UBozorAiMiya)
	int32 GetOxirgiOdob() const { return OxirgiOdob; }
	// oxirgi javob boshidagi munosabat gapi (QopolIzoh/OdobIzoh; yo'q bo'lsa bo'sh). LLM'ga berilmaydi - uni tashlab yuboradi;
	// LLM javobining oldiga o'zgarmay qo'shiladi (u allaqachon sotuvchi fe'li/xulqida)
	const FString& GetOxirgiIzoh() const { return OxirgiIzoh; }
	// oxirgi KutibOl/Bajar javobining ovoz bo'laklari (UBozorTayyorJavob izohi) va sotuvchi uslubi
	const TArray<FString>& GetBolaklar() const { return Bolaklar; }
	const FString& GetUslub() const { return Uslub; }

private:
	FString Javob(EBozorJavob J, int32 Qaytim = 0);
	void MolniTanla(int32 I);

	UPROPERTY() FBozorSuhbatHolati Holat;
	UPROPERTY() TObjectPtr<UBozorHamyon> Hamyon;
	UPROPERTY() TObjectPtr<UBozorSotuvchiMiya> Miya;
	UPROPERTY() TObjectPtr<ABozorNpc> Sotuvchi;
	bool bSotibOldi = false;   // shu sotuvchidan biror narsa oldimi (xayrlashuv gapi uchun)
	EBozorJavob OxirgiJavob = EBozorJavob::KutibOlish;
	int32 OxirgiOdob = 0;
	bool bOdobIzohOldin = false;   // oldingi javobda odobni maqtagan (ketma-ket takrorlamaslik uchun)
	FString OxirgiIzoh;
	TArray<FString> Bolaklar;
	FString Uslub;
	float SalomTalab = 1.f, Tanbeh = 1.f;   // shu sotuvchining odob talabi (FBozorXulq)
	// tanlov oynasi holati
	bool bMiqdorTanlash = false;
	bool bMolRoyxati = false;
	float TanlovMiqdor = 1.f;
	bool bTolov = false;
	TArray<int32> Peshtaxta;   // bola qo'ygan kupyuralar (hali hamyondan olinmagan)
	float MiqdorQadami() const;
};

// Mol ustidagi kichik narx taxtachasi: "pomidor / 12 000 so'm" (o'yin boshida qo'yiladi)
UCLASS()
class BOZORUNREAL_API ABozorNarxTaxta : public AActor
{
	GENERATED_BODY()

public:
	ABozorNarxTaxta();
	void Yoz(const FBozorMahsulot& M);

	UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Taxta;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UTextRenderComponent> Nomi;
	UPROPERTY(VisibleAnywhere) TObjectPtr<class UTextRenderComponent> Narxi;
};

// O'yin boshida sotuv.json ni o'qiydi va har sotuvchi rastasini quradi: fe'l, rasta atrofidagi mollar, shu o'yin narxlari
UCLASS()
class BOZORUNREAL_API UBozorSotuvTizimi : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBozorSotuvTizimi* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	const TArray<FBozorRasta>& GetRastalar() const { return Rastalar; }
	const FBozorRasta* Top(const ABozorNpc* Sotuvchi) const;
	// Joy'ga eng yaqin rasta (markazigacha MaxMasofa sm ichida), yo'q bo'lsa nullptr
	const FBozorRasta* EngYaqin(const FVector& Joy, float MaxMasofa) const;

	// matn ko'rinishida hamma rasta (konsol "Rastalar" uchun)
	FString Hisobot() const;

	// Sotuvchining xotirasi (shu o'yin davomida): suhbat oxiridagi munosabat. Yangi suhbat shundan boshlanadi -
	// qo'pol gapirgan bolani eslab qoladi, odoblisini ham. Biroz kechiradi: boshlang'ich munosabat tomon 30% qaytadi
	void Eslab(const ABozorNpc* Sotuvchi, int32 Munosabat);
	int32 BoshMunosabat(const FBozorRasta& R) const;

	// joriy daraja (?Daraja=N yoki -Daraja=N, 1 dan) va jami darajalar soni
	const FBozorDaraja* GetDaraja() const { return Darajalar.IsValidIndex(DarajaIndeksi) ? &Darajalar[DarajaIndeksi] : nullptr; }
	int32 DarajalarSoni() const { return Darajalar.Num(); }

private:
	void Qur(UWorld& World);
	// har mol ustiga narx taxtachasi; sahnadagi qotib qolgan "6 000" taxtachalar yashiriladi
	void Taxtachalar(UWorld& World);

	UPROPERTY() TArray<FBozorRasta> Rastalar;
	TMap<TWeakObjectPtr<const ABozorNpc>, int32> Xotira;
	UPROPERTY() TArray<FBozorDaraja> Darajalar;
	TMap<FString, TArray<FString>> Gaplar;   // sotuv.json "gaplar" - rastalar Asosiy orqali shunga qaraydi
	int32 DarajaIndeksi = 0;
};

class FJsonObject;

namespace BozorSotuv
{
	// 12000 -> "12 000"
	FString Som(int64 N);
	// 1.5 -> "1,5", 2 -> "2"
	FString Miqdor(float M);
	const TCHAR* FelNomi(EBozorFel F);
	// xulq.json dagi bitta sotuvchi yozuvidan shaxsiy qismi (ism, tavsif, odatlar, namunalar, boshMunosabat)
	void XulqniOqi(const FJsonObject& O, FBozorXulq& X);
}
