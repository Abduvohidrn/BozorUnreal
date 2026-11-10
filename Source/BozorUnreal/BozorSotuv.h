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
	UPROPERTY(BlueprintReadOnly) FVector Markaz = FVector::ZeroVector;   // sotuvchi va navbat orasi
	UPROPERTY(BlueprintReadOnly) FVector Xaridorga = FVector::ForwardVector;   // sotuvchidan xaridor tomonga (gorizontal)

	// fe'lga xos gaplar: javob turi nomi (EBozorJavob) -> variantlar
	TMap<FString, TArray<FString>> Gaplar;
};

UENUM(BlueprintType)
enum class EBozorBosqich : uint8
{
	Boshlanish,
	NarxAytildi,
	PulKutilmoqda,
	QaytimTekshir,   // pul olindi, qaytim berildi - bola sanab, rozi bo'ladi yoki "kam" deydi
	Tugadi
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
	QaytimKam      // "qaytim kam berdingiz"
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
	UPROPERTY(BlueprintReadOnly) float Miqdor = 0.f;
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
	OshBolsin     // qaytimni oldi
};

// Gapni tushunadi va javob matnini yozadi. Hozir - tayyor javoblar; keyin AI bilan almashtiriladi.
UCLASS(Abstract, BlueprintType)
class BOZORUNREAL_API UBozorSotuvchiMiya : public UObject
{
	GENERATED_BODY()

public:
	virtual FBozorTushuncha Tushun(const FString& Gap, const FBozorSuhbatHolati& Holat) PURE_VIRTUAL(UBozorSotuvchiMiya::Tushun, return {};);
	virtual FString Yoz(EBozorJavob Javob, const FBozorSuhbatHolati& Holat, int32 Qaytim) PURE_VIRTUAL(UBozorSotuvchiMiya::Yoz, return {};);
};

// Kalit so'zlar bo'yicha tushunadi, javoblarni tayyor variantlardan tasodifiy tanlaydi.
// Fe'lga xos gaplar (FBozorRasta::Gaplar) bo'lsa, avval shulardan tanlaydi
UCLASS()
class BOZORUNREAL_API UBozorTayyorJavob : public UBozorSotuvchiMiya
{
	GENERATED_BODY()

public:
	virtual FBozorTushuncha Tushun(const FString& Gap, const FBozorSuhbatHolati& Holat) override;
	virtual FString Yoz(EBozorJavob Javob, const FBozorSuhbatHolati& Holat, int32 Qaytim) override;

	TMap<FString, TArray<FString>> FelGaplari;
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

	bool RoyxatTugadimi() const;
	// darajaning puli, ro'yxati va eslatmasi bilan boshidan boshlaydi (statistika ham nolga)
	void Boshla(const FBozorDaraja& D);
	// 1-3 yulduz: 1 - ro'yxat to'liq, +1 - odob (hamma suhbat salom bilan, qo'pol gap yo'q), +1 - qaytim xatosi o'tkazilmagan va savdolashib tejalgan
	int32 Yulduzlar() const;

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
	void Boshla(const FBozorRasta& Rasta, UBozorHamyon* Hamyon, UBozorSotuvchiMiya* Miya);

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

private:
	FString Javob(EBozorJavob J, int32 Qaytim = 0);
	void MolniTanla(int32 I);

	UPROPERTY() FBozorSuhbatHolati Holat;
	UPROPERTY() TObjectPtr<UBozorHamyon> Hamyon;
	UPROPERTY() TObjectPtr<UBozorSotuvchiMiya> Miya;
	UPROPERTY() TObjectPtr<ABozorNpc> Sotuvchi;
	bool bSotibOldi = false;   // shu sotuvchidan biror narsa oldimi (xayrlashuv gapi uchun)
	EBozorJavob OxirgiJavob = EBozorJavob::KutibOlish;
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

	// joriy daraja (?Daraja=N yoki -Daraja=N, 1 dan) va jami darajalar soni
	const FBozorDaraja* GetDaraja() const { return Darajalar.IsValidIndex(DarajaIndeksi) ? &Darajalar[DarajaIndeksi] : nullptr; }
	int32 DarajalarSoni() const { return Darajalar.Num(); }

private:
	void Qur(UWorld& World);
	// har mol ustiga narx taxtachasi; sahnadagi qotib qolgan "6 000" taxtachalar yashiriladi
	void Taxtachalar(UWorld& World);

	UPROPERTY() TArray<FBozorRasta> Rastalar;
	UPROPERTY() TArray<FBozorDaraja> Darajalar;
	int32 DarajaIndeksi = 0;
};

namespace BozorSotuv
{
	// 12000 -> "12 000"
	FString Som(int64 N);
	// 1.5 -> "1,5", 2 -> "2"
	FString Miqdor(float M);
	const TCHAR* FelNomi(EBozorFel F);
}
