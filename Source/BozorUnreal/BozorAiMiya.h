#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BozorSotuv.h"
#include "BozorAiMiya.generated.h"

class ABozorNpc;

// Sotuvchining "tili": LLM har javobni shu sotuvchining fe'li va kayfiyatida qayta yozadi.
// Manba (bozor.MiyaManba): 1 - bulut rag-api.edcom.uz (kalit: AI/miya/kalit.txt yoki BOZOR_AI_KALIT), kalit bo'lmasa yoki
// -AiLokal / bozor.MiyaManba 0 - lokal llama.cpp + Qwen3-4B.
// Nima deyishni (narx, summa, qaytim, rad etish) har doim UBozorSuhbat hal qiladi - LLM faqat ma'noni o'z uslubida aytadi.
// Tekshiruv: tayyor gapdagi har bir son LLM javobida aynan bo'lishi kerak, aks holda tayyor gap ishlatiladi.
// Fayllar: <loyiha>/AI/miya/llama/llama-server.exe (+ dll), AI/miya/model.gguf - AI/miya/yukla.sh tayyorlaydi.
// O'chirish: -AiYoq buyruq qatori yoki bozor.Miya 0.

// Sotuvchining shaxsi (LLM'ga beriladi)
struct FBozorShaxs
{
	FString Id;   // NPC nomi (BozorNpc_267): bulutda shu sotuvchining alohida suhbat sessiyasi (conversation_id) kaliti
	FString Ism;
	int32 Yosh = 45;
	bool bAyol = false;
	EBozorFel Fel = EBozorFel::Mehribon;
	FBozorXulq Xulq;   // xulq.json dagi shaxsiy xulq-atvor (bo'lsa promptda fe'ldan ustun)
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

	// sotuvchining shaxsi: jinsi NPC'dan; ismi, yoshi va xulqi xulq.json dan (bo'lmasa NPC nomidan, har o'yinda bir xil)
	static FBozorShaxs Shaxs(const ABozorNpc* Npc, EBozorFel Fel);

	// Mano (tayyor gap) ni shu sotuvchi uslubida qayta yozadi. Tayyor o'yin oqimida chaqiriladi:
	// bAi=false - LLM javob bermadi, kechikdi yoki son xato (Matn = Mano).
	// Saqla - javobda albatta qolishi kerak bo'lgan so'zlar (mol nomlari). Oxiridagi "[Tarozida: ...]" LLM'ga berilmaydi, qayta qo'shiladi
	// BolaOdob - bolaning shu gapi: -1 qo'pol, 1 odobli (UBozorSuhbat::GetOxirgiOdob) - sotuvchi o'z fe'lida munosabat bildiradi
	// Tarix - oldingi navbatlar (bola, sotuvchi), Savdolashuv - bola necha marta narx tushirishni so'radi: bulut rejimida
	// sotuvchi bolaning gapiga va suhbat oqimiga o'z xarakterida munosabat bildiradi
	void QaytaYoz(const FBozorShaxs& Kim, int32 Munosabat, const FString& BolaGapi, const FString& Mano, const TArray<FString>& Saqla,
		TFunction<void(const FString& Matn, bool bAi)> Tayyor, int32 BolaOdob = 0, const TArray<TPair<FString, FString>>* Tarix = nullptr, int32 Savdolashuv = 0);

	// Erkin suhbat (ovozli chat kabi): bola savdodan tashqari gap aytdi yoki savol berdi ("qayerdansiz?", "charchadingizmi?") -
	// sotuvchi o'z shaxsida, suhbat tarixini bilib javob beradi. Pul hisobi yo'q: javobda son/narx bo'lsa rad etiladi
	// (Zaxira qaytadi, bAi=false). Mollar - rastadagi mollar ("bodring, kartoshka"), gapni savdoga qaytarish uchun
	void Suhbatlash(const FBozorShaxs& Kim, int32 Munosabat, const TArray<TPair<FString, FString>>& Tarix, const FString& BolaGapi,
		const FString& Mollar, const FString& Zaxira, TFunction<void(const FString& Matn, bool bAi)> Tayyor);

	// Bolaning gapini TUSHUNISH (ovozli savdo): LLM suhbat holatini (mollar, bosqich, narx, sotuvchining oxirgi gapi) bilib,
	// gapdan niyatni JSON sxema bo'yicha ajratadi -> FBozorTushuncha. Kalit so'z bilan tushunilgani (Kalit) bilan birlashtiriladi:
	// sonlar kalit so'zdan ishonchliroq (LLM son to'qishi mumkin), qo'pollik ikkalasidan, niyat - LLM'dan (u "boshqa" desa - kalitdan).
	// bOk=false - LLM javob bermadi/kechikdi: Kalit qaytadi
	void Tushun(const FString& BolaGapi, const FBozorSuhbatHolati& Holat, const FString& SotuvchiOxirgiGapi, const FBozorTushuncha& Kalit,
		TFunction<void(const FBozorTushuncha& T, bool bOk)> Tayyor);

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
	// bitta so'rov: bulutda (rag-api) yoki lokal llama-server'da. Natija(Xom javob, HTTP 200 keldimi) - o'yin oqimida.
	// Sessiya - sotuvchi Id si: bulutda har sotuvchining o'z conversation_id si bo'ladi (server suhbat kontekstini o'zi saqlaydi,
	// sotuvchilar bir-birining gapini bilmaydi); bo'sh - sessiyasiz bir martalik so'rov
	void Yubor(const FString& TizimMatn, const TArray<TPair<FString, FString>>& Tarix, const FString& Savol, double Harorat, int32 MaxToken,
		TFunction<void(const FString& Xom, bool bOk)> Natija, const FString& Sessiya = FString());
	// shu sotuvchining bulut sessiyasi ochilganmi (bo'lsa tarix xabarga qo'shilmaydi - server uni biladi)
	bool SessiyaBor(const FString& Sessiya) const { return bBulut && !Sessiya.IsEmpty() && Sessiyalar.Contains(Sessiya); }

	bool bBulut = false;   // true - rag-api.edcom.uz, false - lokal llama-server
	FString Kalit;
	TMap<FString, FString> Sessiyalar;   // sotuvchi Id -> conversation_id (o'yin davomida)
	FProcHandle Jarayon;
	int32 Port = 0;
	bool bTayyor = false;
	double BoshVaqt = 0.0;
	int32 Sorovlar = 0, AiJavoblar = 0;
};
