#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "BozorNavoiy.generated.h"

class USoundWave;
class USoundWaveProcedural;

// Bitta oqimli so'rov holati (/ayt_oqim): Yangi - HTTP ipi yozadi (Qulf ostida), qolgani faqat o'yin oqimida
struct FBozorNavoiyOqim
{
	FCriticalSection Qulf;
	TArray<uint8> Yangi;
	TArray<uint8> Qoldiq;                          // to'liq bo'lmagan sarlavha yoki namunaning toq bayti
	TStrongObjectPtr<USoundWaveProcedural> W;      // oqim tugaguncha ushlab turiladi
	TFunction<void(USoundWave*)> Tayyor;
	FString Nomi;
	double T0 = 0.0, Birinchi = 0.0;
	int64 Bayt = 0;
	int32 Chastota = 24000;
	bool bJavob = false, bSarlavha = false, bYaroqsiz = false, bTugadi = false;
};

// Sotuvchining jonli ovozi: lokal Navoiy TTS (aisha-org/navoiy-tts + CosyVoice2-0.5B, Apache-2.0) har gapni
// shu sotuvchining fe'li va jinsidagi ovozda o'qiydi - LLM qayta yozgan gap ham ovozli bo'ladi.
// Server: <loyiha>/AI/navoiy/server.py (Python, CUDA GPU kerak), muhit AI/navoiy/muhit/ (venv + modellar, git'ga kirmaydi)
// yoki WSL ichida: AI/navoiy/wsl.txt (1-qator distro, 2-qator muhit papkasi, masalan /home/<user>/navoiy) - wsl.exe orqali.
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

	// Matnni ovozlashtiradi (oqim bilan); Tayyor o'yin oqimida birinchi bo'lak kelganda chaqiriladi - tovush keyingi
	// bo'laklar bilan to'lib boradi (Duration ham o'sadi). Ovoz=nullptr - server javob bermadi/birinchi bo'lak kechikdi
	void Ayt(const FString& Fel, bool bAyol, const FString& Matn, TFunction<void(USoundWave* Ovoz)> Tayyor, float Ohang = 0.f, const FString& Namuna = FString());

	// WAV (PCM16) baytlaridan chalinadigan tovush (sinovlar uchun ochiq)
	static USoundWave* WavdanOvoz(const TArray<uint8>& Wav, UObject* Egasi);

	// AI ishlayotganda (LLM javob yozyapti, Navoiy gapni ovozlashtiryapti) o'yin kadrini cheklash (bozor.NavoiyFps) - videokarta
	// AI'ga bo'shasin. So'rovlar soni bo'yicha: har true'ga bitta false; oxirgisi tugaganda eski kadr qaytadi
	void KadrChekla(bool bHa);

private:
	void Ishga();
	void Sogmi(int32 Urinish);
	// -NavoiySinov: Navoiy tayyor bo'lgach har rasta sotuvchisiga bitta gap aytdiradi, chalinishini tekshiradi
	// va o'yinni yopadi; natija logda "[NavoiySinov]" qatorlarida
	void Sinov(int32 I, int32 Ovozli, int32 Chalindi);
	void OqimIshla(const TSharedRef<FBozorNavoiyOqim, ESPMode::ThreadSafe>& O, bool bOxiri);
	int32 Faol = 0;
	float EskiFps = 0.f;
	bool bFpsOzgardi = false;

	FProcHandle Jarayon;
	void* StdinOqi = nullptr;   // WSL rejimi: serverning stdin quvuri (yopilsa server chiqadi)
	void* StdinYoz = nullptr;
	int32 Port = 0;
	bool bTayyor = false;
	double BoshVaqt = 0.0;
	int32 Sorovlar = 0, Ovozlar = 0;
};
