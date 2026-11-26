#pragma once

#include "Blueprint/UserWidget.h"
#include "BozorMobil.generated.h"

class SBozorSensor;
class SEditableTextBox;
class ABozorHud;

namespace BozorMobil
{
	bool Faol();
}

UCLASS()
class UBozorMobil : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& Geometry, float Dt) override;

private:
	ABozorHud* Hud() const;
	void Kamera(const FVector2D& Delta);
	void Mashina();
	void MatnYubor();

	TSharedPtr<SBozorSensor> Joystik;
	TSharedPtr<SEditableTextBox> Matn;
};
