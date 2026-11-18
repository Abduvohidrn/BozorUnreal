#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "BozorSotuv.h"
#include "BozorOyinchi.generated.h"

class UCameraComponent;
class UBozorQuloq;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

// Kaft va barmoqlar animatsiyadan keyin modelning o'z tabiiy pozasiga qaytariladi (bilak to'g'ri, barmoqlar
// yengil bukilgan). Retarget qilingan kliplarda barmoqlar yoyilib, bilak qayrilib qolardi
struct FBozorTabiiyQol
{
	void Qoy(USkeletalMeshComponent* Mesh);

private:
	const USkeletalMesh* Meshi = nullptr;
	TArray<int32> Suyaklar;   // hand_l, hand_r va ularning bolalari (ota-onadan keyin keladi)
	TArray<FTransform> Mahalliy;   // har biriga beriladigan ota-onaga nisbatan poza
};

// Birinchi shaxs qo'llari - Epic First Person shablonidagi kabi: ABP_FP_Copy tananing pozasini nusxalaydi,
// CtrlRig_FPWarp umurtqani orqaga tortib boshni kameraga tekislaydi, qo'llar ko'rish maydoniga tabiiy chiqadi.
// Gavda va bosh yashiriladi - ekranda faqat qo'llar (CS2 dagi kabi)
UCLASS()
class BOZORUNREAL_API UBozorQolMesh : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	virtual void FinalizeBoneTransform() override;

	// O'ng qo'l nishonga cho'ziladi (sotuvchi uzatgan molni olish): QolNishon - dunyo nuqtasi (FP fazosida, ya'ni qo'llar
	// chiziladigan joyda), QolAlfa 0..1 - animatsiyadagi joydan nishongacha
	FVector QolNishon = FVector::ZeroVector;
	float QolAlfa = 0.f;

private:
	void GavdaniYigish();
	void QolniCHoz();

	FBozorTabiiyQol TabiiyQol;
	TArray<int32> Yigiladigan;
	const USkeletalMesh* YigishMeshi = nullptr;
	TArray<int32> OngQolZanjiri;   // upperarm_r ning barcha avlodlari (ota-onadan keyin)
	int32 Yelka = INDEX_NONE, Tirsak = INDEX_NONE, Kaft = INDEX_NONE;
	const USkeletalMesh* IkMeshi = nullptr;
};

// Tana: Auto-Rig Pro skeletida ko'z, lab va boshqa yuz suyaklari boshga emas, ildizga ulangan - animatsiya ularni
// qimirlatmaydi va ko'zlar bosh ichida qolib ketadi. Animatsiyadan keyin ular boshga nisbatan asl joyiga qaytariladi
UCLASS()
class BOZORUNREAL_API UBozorTanaMesh : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	virtual void FinalizeBoneTransform() override;

private:
	void YuzniBoshgaBogla();

	FBozorTabiiyQol TabiiyQol;
	TArray<int32> YuzSuyaklari;
	TArray<FTransform> BoshgaNisbatan;   // ref pozada: yuz suyagi / bosh
	int32 Bosh = INDEX_NONE;
	const USkeletalMesh* YuzMeshi = nullptr;
};

// O'yinchi: 8-10 yoshli bola, birinchi shaxsda. Tana (soya va boshqalar uchun) egasiga ko'rinmaydi,
// qo'llar alohida mesh'da faqat egasiga ko'rinadi
UCLASS()
class BOZORUNREAL_API ABozorOyinchi : public ACharacter
{
	GENERATED_BODY()

public:
	ABozorOyinchi(const FObjectInitializer& ObjectInitializer);

	// bo'y, sm (8-10 yosh: 128-140)
	UPROPERTY(EditAnywhere, Category = "Bozor")
	float Boy = 133.f;

	// ko'zning bosh suyagiga nisbatan joyi (bola modeli o'lchamida, sm): X oldinga, Z yuqoriga
	UPROPERTY(EditAnywhere, Category = "Bozor")
	FVector KozSiljish = FVector(12.f, 0.f, 11.f);

	// kamerada qadam tebranishining qancha qismi qoladi (X oldinga-orqaga, Y yonga, Z tik); 1 - animatsiyadagidek
	UPROPERTY(EditAnywhere, Category = "Bozor")
	FVector TebranishKuchi = FVector(0.5f, 0.3f, 0.6f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UCameraComponent> Kamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UBozorQolMesh> Qollar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UBozorHamyon> Hamyon;

	// mikrofon: V ni bosib turib gapiriladi, lokal STT matni Gapir() ga boradi
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UBozorQuloq> Quloq;

	// sotuvchi bilan gaplashish mumkin bo'lgan masofa (rasta markazigacha, sm)
	UPROPERTY(EditAnywhere, Category = "Bozor")
	float GaplashishMasofasi = 350.f;

	// Sinov uchun konsol: Gapir <matn> - eng yaqin sotuvchiga gap (tanlov oynasi bo'lguncha)
	UFUNCTION(Exec)
	void Gapir(const FString& Matn);

	// Konsol: eng yaqin sotuvchi bilan suhbatni qaytadan boshlash
	UFUNCTION(Exec)
	void YangiSotuvchi();

	UFUNCTION(Exec)
	void HamyonKor();

	// Konsol: hamma rasta, fe'li, mollari va narxlari
	UFUNCTION(Exec)
	void Rastalar();

	// E: yaqindagi sotuvchi bilan suhbatni boshlaydi yoki tugatadi
	void EBosildi();

	// V: mikrofon bilan gapirish (bosib turiladi)
	void VBosildi();
	void VQoyildi();
	// Konsol: QuloqSinov <audio fayl> - mikrofonsiz, faylni STT dan o'tkazib sotuvchiga aytadi
	UFUNCTION(Exec)
	void QuloqSinov(const FString& Fayl);
	const UBozorQuloq* GetQuloq() const { return Quloq; }
	// Sozlamalardagi tugmalar o'zgarganda: moslashgan IMC'ni qayta qo'yadi (BozorSozlamalar)
	void KirishniYangila();

	// Javob variantini tanlash (0 dan; tugma, 1-5 raqamlar yoki konsol "Tanla 0")
	UFUNCTION(Exec)
	void Tanla(int32 I);

	TArray<FBozorTanlov> GetTanlovlar() const;

	// Natija oynasi (ro'yxat tugab chiqishga qaytganda; konsol "Natija" - darhol). Enter - qaytadan o'ynash
	UFUNCTION(Exec)
	void Natija();
	bool NatijaKorinadi() const { return bNatija; }
	const UBozorSuhbat* GetSuhbat() const { return Suhbat; }

	// HUD uchun
	class ABozorNpc* GetYaqinSotuvchi() const { return YaqinSotuvchi.Get(); }
	bool Suhbatda() const { return bSuhbatda; }
	bool Xayrlashdi() const { return TugatishVaqti > 0.0; }
	const FString& GetSotuvchiGapi() const { return SotuvchiGapi; }
	const FString& GetOxirgiGapim() const { return OxirgiGapim; }
	const FString& GetXabar() const { return Xabar; }
	double GetGapVaqti() const { return GapVaqti; }
	double GetXabarVaqti() const { return XabarVaqti; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void TugmaBosildi(FKey Tugma);   // qayta tayinlanadigan amallar (Gaplashish, Qaytadan) va Esc
	void KameraJoyi(float Dt);
	// log va Saved/Sotuv_suhbat.txt; bEkran - ekranga ham (konsol buyruqlari uchun)
	void Chiqar(const FString& Kim, const FString& Matn, const FColor& Rang, bool bEkran = false);
	void RastalarniChiqar(bool bEkran);
	void SotuvTestQadami();
	// yaqindagi (qarab turgan) sotuvchi bilan suhbat boshlaydi; sotuvchi yo'q yoki band bo'lsa false
	bool SuhbatniBoshla();
	void SuhbatniTugat();
	// sotuvchi javobi: matn, harakat va ovoz. AI miya bo'lsa - matnni sotuvchining fe'lida qayta yozdirib, keyin ko'rsatadi
	void SotuvchiAytdi(const FString& Matn);
	void SotuvchiMatni(const FString& Matn);
	// Mol uzatish sahnasi (pul olingach): sotuvchi V_Handout bilan molni (kilo - xaltada, dona - o'zi) uzatadi, bola o'ng qo'lini
	// cho'zib oladi, bir zum ushlab turadi, keyin sumkasiga soladi. Qo'llar FP fazosida chiziladi (FOV 70, masshtab 0.6) -
	// dunyo nuqtasi bilan qo'l bir joyda ko'rinishi uchun FpGa/DunyoGa kamera fazosida o'giradi
	void MolBer(ABozorNpc* V);
	void MolYangila(float Dt);
	void MolTozala();
	FVector MolQolJoyi() const;
	bool MolYetadimi(const FVector& Dunyo) const;
	FVector FpGa(const FVector& Dunyo, float Masofa) const;
	FVector DunyoGa(const FVector& Fp) const;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> BerilganMol;
	TWeakObjectPtr<ABozorNpc> MolSotuvchi;
	double MolBosh = -1.0;
	// sotuvchi V_Handout klipini boshlagan payt va kim (MolBosh uzoqdagi "pastdan" holatda oldinga suriladi, bu - yo'q):
	// mol qo'ldan chiqquncha sotuvchining keyingi harakati kutadi
	TWeakObjectPtr<ABozorNpc> UzatishSotuvchi;
	double UzatishBosh = -1.0;
	float MolH = 2.5f, MolOlcham = 1.f;
	FVector MolUchishBosh = FVector::ZeroVector;   // ushlash paytida mol tutqichining kaftdan farqi (FP fazosida; asta yo'qoladi)
	FVector MolQolBosh = FVector::ZeroVector;   // ushlash paytida bola qo'lining nishoni (u yerdan o'ngga - ushlab turish joyiga)
	FQuat MolBurBosh = FQuat::Identity;         // ushlash paytidagi burilishi (u yerdan kameraga nisbatan tik holatga)
	float MolSBosh = 1.f;                       // ushlash paytidagi FP o'lchami (ekranda sotuvchi qo'lidagidek ko'rinadi)
	FVector MolTutqich = FVector::ZeroVector;   // mesh'ning qo'lda ushlanadigan nuqtasi (xalta - dastasi/tepasi, dona - markazi)
	bool bMolFp = false;
	// yakuniy gapni (LLM qayta yozgan bo'lsa ham) Navoiy bilan ovozlashtiradi; bo'lmasa - yozilgan bo'lakli ovoz (Asl gap bo'yicha)
	void JonliAyt(int32 Raqam, const FString& Matn, const FString& Asl);
	void YaqinSotuvchiniTop();
	void SichqonchaBosildi();
	void KursorniKorsat(bool bHa);
	void TestRastasigaBor();

	UPROPERTY() TObjectPtr<UInputMappingContext> ImcDefault;
	UPROPERTY() TObjectPtr<UInputMappingContext> ImcMouse;
	UPROPERTY() TObjectPtr<UInputAction> IaMove;
	UPROPERTY() TObjectPtr<UInputAction> IaLook;
	UPROPERTY() TObjectPtr<UInputAction> IaMouseLook;
	UPROPERTY() TObjectPtr<UInputAction> IaJump;

	UPROPERTY() TObjectPtr<UBozorSuhbat> Suhbat;

	TWeakObjectPtr<class ABozorNpc> YaqinSotuvchi;   // E bosilsa shu bilan gaplashadi
	bool bSuhbatda = false;
	FString SotuvchiGapi, OxirgiGapim, Xabar;
	double GapVaqti = -100.0, XabarVaqti = -100.0;
	double TugatishVaqti = -1.0;   // xayrlashgandan keyin suhbat o'zi yopiladigan payt
	int32 MiyaSorov = 0;           // AI miyaga oxirgi so'rov raqami (eskisining javobi kelsa - tashlanadi)
	// ovozli chat: o'yinchi gapira boshladi (qo'lsiz rejim) - sotuvchi gapirayotgan bo'lsa, gapi bo'linadi
	void GapBoshlandi();
	// shu suhbatning tarixi (bola gapi, sotuvchi javobi) - erkin suhbatda LLM oldingi gaplarni eslasin
	TArray<TPair<FString, FString>> Tarix;
	bool bNatija = false;
	void EnterBosildi();
	void RBosildi();
	// natijadan keyin: o'tilgan bo'lsa keyingi daraja, bo'lmasa shu daraja qaytadan
	void DarajagaOt(bool bKeyingi);
	bool RastagaBor(const FBozorRasta& R);

	// -DarajaTest: bot ro'yxatni o'zi bajaradi (eng arzon rasta, odobli savdo, kupyura, qaytim) va chiqishga qaytadi
	void BotQadami();
	bool bBot = false;
	double BotKeyingi = 0.0;
	int32 BotBerildi = 0, BotSumma = 0;
	FString BotMol;          // hozir qaysi mol olinmoqda
	TArray<FString> BotTashlandi;   // olib bo'lmagan mollar (pul yetmadi va h.k.)
	FVector BotBoshJoy = FVector::ZeroVector;
	int32 BotBosqich = 0;    // 0 - xarid, 1 - chiqishga ketdi, 2 - natija surati, 3 - chiqish

	// boshning kapsulaga nisbatan sekin o'rtacha joyi (kamera tebranishini yumshatish uchun)
	FVector KameraOrtacha = FVector::ZeroVector;
	bool bKameraTayyor = false;

	// -SotuvTest: o'zi yurib, tayyor suhbatni o'tkazadi va suratga oladi
	bool bTest = false;
	int32 TestQadam = 0;
	TArray<FString> TestGaplar;
	// tanlov sinovi: har qadamda shu shartga mos variant tanlanadi
	TArray<TFunction<bool(const FBozorTanlov&)>> TestTanlovlar;
	TArray<FString> TestSuratlar;   // qadam indeksi bo'yicha surat nomi (bo'sh - suratsiz)
	double TestVaqt = 0.0;
	double TestVaqt2 = 0.0;
	bool bTestSuratE = false, bTestSuratKutib = false;
	// -SotuvTest -AiSinov: LLM va Navoiy tayyor bo'lishini kutadi, keyingi tanlov - sotuvchi gapini aytib bo'lgach
	bool bJonliKutish = false;     // Navoiy ovozi kutilmoqda
	double AytVaqt = 0.0;          // sotuvchi javobi boshlangan payt (LLM + Navoiy kechikishini o'lchash)
	double TestKeyingi = 0.0, TestKutishBosh = -1.0;
};

UCLASS()
class BOZORUNREAL_API ABozorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABozorGameMode();
};
