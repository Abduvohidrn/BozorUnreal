#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BozorSotuv.h"
#include "BozorAiMiya.generated.h"

class ABozorNpc;

// Sotuvchining "tili": lokal LLM (llama.cpp + Qwen3-4B) har javobni shu sotuvchining fe'li va kayfiyatida qayta yozadi.
// Nima deyishni (narx, summa, qaytim, rad etish) har doim UBozorSuhbat hal qiladi - LLM faqat ma'noni o'z uslubida aytadi.
// Tekshiruv: tayyor gapdagi har bir son LLM javobida aynan bo'lishi kerak, aks holda tayyor gap ishlatiladi.
// Fayllar: <loyiha>/AI/miya/llama/llama-server.exe (+ dll), AI/miya/model.gguf - AI/miya/yukla.sh tayyorlaydi.
// O'chirish: -AiYoq buyruq qatori yoki bozor.Miya 0.

// Sotuvchining shaxsi (LLM'ga beriladi)
struct FBozorShaxs
{
	FString Ism;
	int32 Yosh = 45;
	bool bAyol = false;
	EBozorFel Fel = EBozorFel::Mehribon;
};

UCLASS()
class BOZORUNREAL_API UBozorAiMiya : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UBozorAiMiya* Get(const UObject* WorldContext);

	// server ishga tushib, model yuklandimi
	bool Tayyormi() const { return bTayyor; }
	// AI fayllari joyidami
	static bool FayllarBormi();

	// sotuvchining shaxsi: jinsi va yoshi NPC'dan, ismi NPC nomidan (har o'yinda bir xil)
	static FBozorShaxs Shaxs(const ABozorNpc* Npc, EBozorFel Fel);

	// Mano (tayyor gap) ni shu sotuvchi uslubida qayta yozadi. Tayyor o'yin oqimida chaqiriladi:
	// bAi=false - LLM javob bermadi, kechikdi yoki son xato (Matn = Mano).
	// Saqla - javobda albatta qolishi kerak bo'lgan so'zlar (mol nomlari). Oxiridagi "[Tarozida: ...]" LLM'ga berilmaydi, qayta qo'shiladi
	void QaytaYoz(const FBozorShaxs& Kim, int32 Munosabat, const FString& BolaGapi, const FString& Mano, const TArray<FString>& Saqla,
		TFunction<void(const FString& Matn, bool bAi)> Tayyor);

	// serverni qo'lda boshlash/to'xtatish (Bozor.Miya sinovi uchun; o'yinda Initialize/Deinitialize qiladi)
	bool Boshla();
	void Toxtat();

	// LLM javobini tozalash va tekshirish (sinovlar uchun ochiq)
	static FString Tozala(const FString& Xom, const FBozorShaxs& Kim);
	static bool SonlarMosmi(const FString& Mano, const FString& Javob);

private:
	void Ishga();
	void Sogmi(int32 Urinish);
	FString Tizim(const FBozorShaxs& Kim) const;

	FProcHandle Jarayon;
	int32 Port = 0;
	bool bTayyor = false;
	double BoshVaqt = 0.0;
	int32 Sorovlar = 0, AiJavoblar = 0;
};
