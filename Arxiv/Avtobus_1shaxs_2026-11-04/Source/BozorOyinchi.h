#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "BozorSotuv.h"
#include "BozorOyinchi.generated.h"

class UCameraComponent;
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

private:
	void GavdaniYigish();

	FBozorTabiiyQol TabiiyQol;
	TArray<int32> Yigiladigan;
	const USkeletalMesh* YigishMeshi = nullptr;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UCameraComponent> Kamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UBozorQolMesh> Qollar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bozor")
	TObjectPtr<UBozorHamyon> Hamyon;

	// hozircha bitta rasta mahsuloti; keyin rastaga qarab olinadi
	UPROPERTY(EditAnywhere, Category = "Bozor")
	FBozorMahsulot Mahsulot;

	// Konsol: Gapir <matn> - sotuvchiga gap (chat oynasi bo'lguncha)
	UFUNCTION(Exec)
	void Gapir(const FString& Matn);

	// Konsol: yangi suhbat (boshqa sotuvchiga o'tgandek)
	UFUNCTION(Exec)
	void YangiSotuvchi();

	UFUNCTION(Exec)
	void HamyonKor();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void KameraJoyi();
	void Chiqar(const FString& Kim, const FString& Matn, const FColor& Rang);
	void SotuvTestQadami();

	UPROPERTY() TObjectPtr<UInputMappingContext> ImcDefault;
	UPROPERTY() TObjectPtr<UInputMappingContext> ImcMouse;
	UPROPERTY() TObjectPtr<UInputAction> IaMove;
	UPROPERTY() TObjectPtr<UInputAction> IaLook;
	UPROPERTY() TObjectPtr<UInputAction> IaMouseLook;
	UPROPERTY() TObjectPtr<UInputAction> IaJump;

	UPROPERTY() TObjectPtr<UBozorSuhbat> Suhbat;

	// -SotuvTest: o'zi yurib, tayyor suhbatni o'tkazadi va suratga oladi
	bool bTest = false;
	int32 TestQadam = 0;
	double TestVaqt = 0.0;
	double TestVaqt2 = 0.0;
};

UCLASS()
class BOZORUNREAL_API ABozorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABozorGameMode();
};
