#pragma once

#include "CoreMinimal.h"

// Ikkinchi STT: rubaiSTT v2 medium (Whisper Medium asosida, Apache-2.0) - transcribe.cpp (MIT) kutubxonasi orqali,
// o'yin jarayoni ichida (har gapda yangi jarayon ochilmaydi). Videokarta (Vulkan) bo'lsa unda, bo'lmasa CPU'da.
// Fayllar: <loyiha>/AI/stt/rubai/ (transcribe.dll, ggml*.dll, rubaistt.gguf) - AI/stt/yukla.sh rubai tayyorlaydi.
// Tanlash: bozor.Stt 1 yoki -SttRubai (UBozorQuloq ishlatadi).
namespace BozorRubai
{
	// fayllar joyidami
	bool Bormi();
	// rubaiSTT tanlanganmi (bozor.Stt / -SttRubai) va fayllari bormi
	bool Tanlangan();
	// modelni oldindan yuklash (fon ipida chaqirish mumkin; birinchi gap kutmasin). false - yuklanmadi
	bool Yukla();
	// 16 kHz mono float PCM -> matn. Bloklaydi (fon ipida chaqiring); bir vaqtda bitta gap. bOk=false - xato
	FString Tanit(const TArray<float>& Pcm, bool& bOk);
	// 16 kHz 16-bit mono WAV o'qish (boshqa format bo'lsa false)
	bool WavOqi(const FString& Fayl, TArray<float>& Pcm);
	// modelni bo'shatish (o'yin yopilganda)
	void Bosh();
}
