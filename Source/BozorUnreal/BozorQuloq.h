#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BozorQuloq.generated.h"

namespace Audio { class FAudioCaptureSynth; }

// O'yinchining "qulog'i": mikrofondan o'zbekcha gapni yozib, lokal STT (CrispASR + NVIDIA FastConformer uz)
// bilan matnga aylantiradi. Internet ham, videokarta ham kerak emas: STT alohida jarayon, faqat CPU.
// Fayllar: <loyiha>/AI/stt/ (crispasr.exe, openblas.dll, stt-uz.gguf) - AI/stt/yukla.sh tayyorlaydi.
// Natija (matn) NatijaKeldi orqali o'yin oqimida qaytadi; bo'sh matn - eshitilmadi yoki xato.
UCLASS(ClassGroup = Bozor, meta = (BlueprintSpawnableComponent))
class BOZORUNREAL_API UBozorQuloq : public UActorComponent
{
	GENERATED_BODY()

public:
	UBozorQuloq();

	// tugma bosildi: yozishni boshlaydi (mikrofon birinchi marta shu yerda ochiladi). false - mikrofon yoki STT yo'q
	bool Boshla();
	// tugma qo'yildi: yozuv STT ga yuboriladi
	void Tugat();
	// suhbat davomida mikrofon fonda ochiq turadi (faqat oxirgi 0.5 s xotirada, hech qayerga yuborilmaydi):
	// birinchi gapning boshi qurilma ishga tushishiga yo'qolmaydi va tugma bosilishidan oldingi bo'g'in ham yoziladi
	void Isit(bool bHa);
	// sinov: tayyor audio faylni (wav/mp3) xuddi shu yo'l bilan matnga aylantirish
	void Fayldan(const FString& Fayl);

	// tugma bosib turilibdi (oxirgi 0.35 s "dum" yozilayotganda - false)
	bool Tinglayapti() const { return bTinglayapti && !bOxiri; }
	bool Tushunyapti() const { return bTushunyapti || bOxiri; }
	// hozirgi ovoz balandligi 0..1 (HUD uchun)
	float GetBalandlik() const { return Balandlik; }
	// Boshla() false qaytarsa - sababi
	const FString& GetXato() const { return Xato; }
	// STT fayllari joyidami
	static bool Tayyormi();

	TFunction<void(const FString& Matn)> NatijaKeldi;

	// eng uzun gap, s
	UPROPERTY(EditAnywhere, Category = "Bozor")
	float EngUzun = 12.f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Yigish();
	bool Och();
	// "dum" yozilib bo'lgach: kanal tanlash, 16 kHz, WAV, STT
	void Yakunla();
	// STT jarayoni (alohida ip): Audio -> Asos.txt -> NatijaKeldi (o'yin oqimida)
	void Tanit(const FString& Audio, const FString& Asos, float Sekund);

	TSharedPtr<Audio::FAudioCaptureSynth> Synth;
	TArray<float> Xom;      // qurilma chastotasida, kanallar aralash (interleaved)
	TArray<float> Bufer;
	bool bOxiri = false;    // tugma qo'yildi, "dum" yozilmoqda
	bool bYozmoqda = false; // qurilmadan ovoz olinmoqda (tugma yoki fon)
	bool bFon = false;      // suhbat davomida fonda tinglash yoqilgan
	double OxiriVaqti = 0.0;
	int32 Kanallar = 1;
	int32 Chastota = 48000;
	bool bTinglayapti = false;
	bool bTushunyapti = false;
	float Balandlik = 0.f;
	int32 Raqam = 0;
	FString Xato;
};
