#include "BozorOgizMorph.h"
#include "Engine/SkeletalMesh.h"

#if WITH_EDITOR
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#endif

const FName UBozorOgizMorph::MorphNomi(TEXT("Ogiz_Ochiq"));

#if WITH_EDITOR
namespace
{
	FString GProfil;   // oxirgi Topish'ning old profili (burun uchidan ichkariga, sm) - hisobot uchun
	// Lablar payvandlangan (og'iz ichi yo'q): ochilganda teri cho'ziladi. O'tish chizig'i lablar orasidagi qora chiziq ustida
	// bo'lsa - cho'zilgan joy qorong'i bo'lib, og'iz ochilgandek ko'rinadi (0.43 da pastki labning pushti rangi cho'zilardi)
	float OgizNisbat = 0.413f;   // 0.43 - pastki lab cho'ziladi, 0.395 - yuqori lab cho'ziladi

	float Silliq(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	// Og'iz parametrlari (mesh fazosi, sm): bosh +Y tomonga qaraydi, +Z - yuqori (NPC mesh'i -90 yaw bilan qo'yilgan)
	struct FOgiz
	{
		float OgizZ = 0.f, IyakZ = 0.f, BurunZ = 0.f, OldY = 0.f, BogimY = 0.f, BogimZ = 0.f;
		bool bTopildi = false;
	};

	FOgiz Topish(const FMeshDescription& MD)
	{
		FOgiz O;
		TVertexAttributesConstRef<FVector3f> P = MD.GetVertexPositions();
		float MaxY = -1e9f, BurunZ = 0.f, MinY = 1e9f;
		for (const FVertexID V : MD.Vertices().GetElementIDs())
		{
			const FVector3f& X = P[V];
			if (FMath::Abs(X.X) < 2.f && X.Y > MaxY) { MaxY = X.Y; BurunZ = X.Z; }
		}
		if (MaxY < -1e8f) return O;
		// old profil: burundan 18 sm pastgacha har 0.5 sm da o'rta chiziqning eng oldingi nuqtasi
		const int32 PN = 36;
		TArray<float> Old;
		Old.Init(-1e9f, PN);
		for (const FVertexID V : MD.Vertices().GetElementIDs())
		{
			const FVector3f& X = P[V];
			if (FMath::Abs(X.X) > 1.5f || X.Z > BurunZ || X.Z < BurunZ - 18.f) continue;
			const int32 I = FMath::Clamp(int32((BurunZ - X.Z) / 0.5f), 0, PN - 1);
			Old[I] = FMath::Max(Old[I], X.Y);
		}
		GProfil.Reset();
		for (int32 I = 0; I < PN; ++I) GProfil += Old[I] < -1e8f ? TEXT(" -") : FString::Printf(TEXT(" %.1f"), MaxY - Old[I]);
		// iyak: burundan pastga yurib, old profil burun uchidan 4.5 sm dan ko'p ichkariga ketgan joy (iyak osti - bo'yin)
		float IyakZ = BurunZ - 8.f;
		for (int32 I = 6; I < PN; ++I)
			if (Old[I] > -1e8f && MaxY - Old[I] > 4.5f) { IyakZ = BurunZ - I * 0.5f; break; }
		for (const FVertexID V : MD.Vertices().GetElementIDs())
			if (P[V].Z > IyakZ) MinY = FMath::Min(MinY, P[V].Y);
		const float H = BurunZ - IyakZ;
		if (H < 2.f) return O;
		// og'iz chizig'i: burun uchi - iyak osti oralig'ining 43% pastida. Old profildagi chuqurlik og'iz emas, lab ostidagi burma
		// ekan (~60%); 43% SK_ma_head_05_asian'ning yaqin suratida lablar chizig'idan o'lchangan (2026-11-16)
		O.OgizZ = BurunZ - OgizNisbat * H;
		O.IyakZ = IyakZ;
		O.BurunZ = BurunZ;
		O.OldY = MaxY;
		// jag' bo'g'imi: quloq oldi - boshning chuqurligining ~55% ichkarisida, og'izdan biroz yuqorida
		O.BogimY = MaxY - 0.55f * (MaxY - MinY);
		O.BogimZ = O.OgizZ + 0.45f * (BurunZ - O.OgizZ);
		O.bTopildi = true;
		return O;
	}
}
#endif

FString UBozorOgizMorph::OgizQosh(USkeletalMesh* Mesh, float Burchak)
{
#if WITH_EDITOR
	if (!Mesh) return TEXT("mesh yo'q");
	FString Hisobot = Mesh->GetName();
	FOgiz O;
	for (int32 Lod = 0; Lod < Mesh->GetLODNum(); ++Lod)
	{
		FMeshDescription* MD = Mesh->HasMeshDescription(Lod) ? Mesh->GetMeshDescription(Lod) : Mesh->CreateMeshDescription(Lod);
		if (!MD) { Hisobot += FString::Printf(TEXT(" | LOD%d: mesh description yo'q"), Lod); continue; }
		if (Lod == 0)
		{
			O = Topish(*MD);
			if (!O.bTopildi) return Hisobot + TEXT(": og'iz topilmadi");
			// og'iz chizig'idagi ustma-ust uchlar (lablar alohida bo'lsa - juft, payvandlangan bo'lsa - yo'q)
			TVertexAttributesConstRef<FVector3f> P0 = MD->GetVertexPositions();
			TArray<FVector3f> Chiziq;
			for (const FVertexID V : MD->Vertices().GetElementIDs())
				if (FMath::Abs(P0[V].Z - O.OgizZ) < 0.6f && P0[V].Y > O.OldY - 4.f) Chiziq.Add(P0[V]);
			int32 Juft = 0;
			for (int32 I = 0; I < Chiziq.Num(); ++I)
				for (int32 K = I + 1; K < Chiziq.Num(); ++K) Juft += FVector3f::DistSquared(Chiziq[I], Chiziq[K]) < 0.0004f ? 1 : 0;
			Hisobot += FString::Printf(TEXT(" iyak=%.1f og'iz=%.1f burun=%.1f old=%.1f bo'g'im=(%.1f, %.1f) og'iz atrofida %d uch, %d ustma-ust\n  profil:%s\n "),
				O.IyakZ, O.OgizZ, O.BurunZ, O.OldY, O.BogimY, O.BogimZ, Chiziq.Num(), Juft, *GProfil);
		}
		Mesh->ModifyMeshDescription(Lod);
		FSkeletalMeshAttributes A(*MD);
		A.Register();
		if (A.GetMorphTargetNames().Contains(MorphNomi)) A.UnregisterMorphTargetAttribute(MorphNomi);
		A.RegisterMorphTargetAttribute(MorphNomi, false);
		TVertexAttributesRef<FVector3f> Delta = A.GetVertexMorphPositionDelta(MorphNomi);
		TVertexAttributesConstRef<FVector3f> P = MD->GetVertexPositions();
		const float Rad = FMath::DegreesToRadians(Burchak);
		int32 Qimirladi = 0;
		for (const FVertexID V : MD->Vertices().GetElementIDs())
		{
			const FVector3f X = P[V];
			// pastki lab va jag' (og'iz chizig'idan past), jag' bo'g'imidan oldinda, bo'yinga o'tganda so'nadi
			const float W = Silliq(O.OgizZ + 0.12f, O.OgizZ - 0.12f, X.Z)
				* Silliq(O.BogimY - 1.f, O.BogimY + 3.f, X.Y)
				* Silliq(O.IyakZ - 7.f, O.IyakZ - 1.5f, X.Z);
			if (W <= 0.001f) { Delta[V] = FVector3f::ZeroVector; continue; }
			// X o'qi atrofida burish (iyak pastga va sal orqaga tushadi)
			const float Bur = -Rad * W;
			const float Dy = X.Y - O.BogimY, Dz = X.Z - O.BogimZ;
			const float Ny = Dy * FMath::Cos(Bur) - Dz * FMath::Sin(Bur);
			const float Nz = Dy * FMath::Sin(Bur) + Dz * FMath::Cos(Bur);
			Delta[V] = FVector3f(0.f, Ny - Dy, Nz - Dz);
			++Qimirladi;
		}
		USkeletalMesh::FCommitMeshDescriptionParams Params;
		Params.bMarkPackageDirty = true;
		Mesh->CommitMeshDescription(Lod, Params);
		Hisobot += FString::Printf(TEXT(" | LOD%d: %d/%d uch"), Lod, Qimirladi, MD->Vertices().Num());
	}
	Mesh->PostEditChange();
	Hisobot += FString::Printf(TEXT(" | morphlar: %d"), Mesh->GetMorphTargets().Num());
	return Hisobot;
#else
	return TEXT("faqat muharrirda");
#endif
}
