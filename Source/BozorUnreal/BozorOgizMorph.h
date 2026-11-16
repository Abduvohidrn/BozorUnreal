#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BozorOgizMorph.generated.h"

class USkeletalMesh;

// Citizen NPC boshlarida yuz suyagi ham, morph target ham yo'q (68 suyak, faqat "head"). Gapirganda og'iz qimirlashi uchun
// shu vosita har bir bosh mesh'iga "Ogiz_Ochiq" morph target'ini geometriyadan hisoblab yozadi: og'iz chizig'i old profildan
// topiladi, undan pastdagi jag' jag' bo'g'imi atrofida burib ochiladi (bo'yin va boshning orqasi qimirlamaydi).
// O'yinda ABozorSotuvchiOvozi ovoz balandligiga qarab SetMorphTarget("Ogiz_Ochiq", 0..1) qiladi.
// Ishlatish (muharrir yoki commandlet): Scripts/ogiz_morph.py -> unreal.BozorOgizMorph.ogiz_qosh(mesh, burchak)
UCLASS()
class BOZORUNREAL_API UBozorOgizMorph : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static const FName MorphNomi;

	// Mesh'ning barcha LOD'lariga morph yozadi (bor bo'lsa - qayta hisoblaydi). Natija: log matni (og'iz balandligi, qimirlagan uchlar)
	UFUNCTION(BlueprintCallable, Category = "Bozor|Ogiz")
	static FString OgizQosh(USkeletalMesh* Mesh, float Burchak = 16.f);
};
