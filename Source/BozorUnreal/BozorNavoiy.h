#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BozorNavoiy.generated.h"

class USoundWave;

// Sotuvchining jonli ovozi: lokal Navoiy TTS (aisha-org/navoiy-tts + CosyVoice2-0.5B, Apache-2.0) har gapni
// shu sotuvchining fe'li va jinsidagi ovozda o'qiydi - LLM qayta yozgan gap ham ovozli bo'ladi.
// Server: <loyiha>/AI/navoiy/server.py (Python, CUDA GPU kerak), muhit AI/navoiy/muhit/ (venv + modellar, git'ga kirmaydi).
// Ovoz namunalari AI/navoiy/ovozlar/<fel>_<ayol|erkak>.wav, fe'l -> kayfiyat AI/navoiy/ovozlar.json.
// Server ishlamasa yoki kechiksa chaqiruvchi yozilgan bo'lakli ovozga qaytadi (ABozorSotuvchiOvozi::SayLine).
// O'chirish: -NavoiyYoq (yoki -AiYoq) buyruq qatori, bozor.Navoiy 0.
UCLASS()
class BOZORUNREAL_API UBozorNavoiy : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UBozorNavoiy* Get(const UObject* WorldContext);

	// server modellarni yuklab bo'ldimi va bozor.Navoiy yoqiqmi
	bool Tayyormi() const;
	static bool FayllarBormi();

	// Matnni ovozlashtiradi; Tayyor o'yin oqimida chaqiriladi: Ovoz=nullptr - server javob bermadi/kechikdi
	void Ayt(const FString& Fel, bool bAyol, const FString& Matn, TFunction<void(USoundWave* Ovoz)> Tayyor);

	// WAV (PCM16) baytlaridan chalinadigan tovush (sinovlar uchun ochiq)
	static USoundWave* WavdanOvoz(const TArray<uint8>& Wav, UObject* Egasi);

private:
	void Ishga();
	void Sogmi(int32 Urinish);
	// -NavoiySinov: Navoiy tayyor bo'lgach har rasta sotuvchisiga bitta gap aytdiradi, chalinishini tekshiradi
	// va o'yinni yopadi; natija logda "[NavoiySinov]" qatorlarida
	void Sinov(int32 I, int32 Ovozli, int32 Chalindi);

	FProcHandle Jarayon;
	int32 Port = 0;
	bool bTayyor = false;
	double BoshVaqt = 0.0;
	int32 Sorovlar = 0, Ovozlar = 0;
};
