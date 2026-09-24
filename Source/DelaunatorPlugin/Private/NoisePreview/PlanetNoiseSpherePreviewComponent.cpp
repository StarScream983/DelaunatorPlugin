#include "NoisePreview/PlanetNoiseSpherePreviewComponent.h"
#include "Async/ParallelFor.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "TimerManager.h"

#ifdef IMGUI_API
#include "ImGuiModule.h"
#include "ImGuiTextureHandle.h"
#include <imgui.h>
#endif

UPlanetNoiseSpherePreviewComponent::UPlanetNoiseSpherePreviewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = false;
}

void UPlanetNoiseSpherePreviewComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UPlanetNoiseSpherePreviewComponent::RegeneratePreview);
	}
}

void UPlanetNoiseSpherePreviewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseImGuiTexture();
	Super::EndPlay(EndPlayReason);
}

void UPlanetNoiseSpherePreviewComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bShowImGui)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	if (LastImGuiDrawFrame == GFrameCounter)
	{
		return;
	}
	LastImGuiDrawFrame = GFrameCounter;

	DrawImGui();
}

PlanetNoise::EBase UPlanetNoiseSpherePreviewComponent::ToBase(EPlanetNoiseBaseType T) const
{
	switch (T)
	{
	case EPlanetNoiseBaseType::Perlin: return PlanetNoise::EBase::Perlin;
	case EPlanetNoiseBaseType::Value:  return PlanetNoise::EBase::Value;
	case EPlanetNoiseBaseType::Simplex:
	default:                           return PlanetNoise::EBase::Simplex;
	}
}

// Orbis CloudCoverageMap.usf GetCubeDirection — face order must stay consistent with UE TextureCube.
FVector3f UPlanetNoiseSpherePreviewComponent::GetCubeDirection(int32 Face, float FaceU, float FaceV) const
{
	switch (Face)
	{
	case 0:  return FVector3f(1.f, -FaceV, -FaceU).GetSafeNormal();  // +X
	case 1:  return FVector3f(-1.f, -FaceV, FaceU).GetSafeNormal();  // -X
	case 2:  return FVector3f(FaceU, 1.f, FaceV).GetSafeNormal();   // +Y
	case 3:  return FVector3f(FaceU, -1.f, -FaceV).GetSafeNormal(); // -Y
	case 4:  return FVector3f(FaceU, -FaceV, 1.f).GetSafeNormal();  // +Z
	default: return FVector3f(-FaceU, -FaceV, -1.f).GetSafeNormal(); // -Z
	}
}

float UPlanetNoiseSpherePreviewComponent::SampleCoverage(const FVector3f& SurfacePos) const
{
	const float PlanetDiameter = FMath::Max(2.f * OuterRadius, 1.f);
	const PlanetNoise::EBase CovBase = ToBase(CoverageBase);

	const float CoverageFrequency = CoverageNoiseScale / PlanetDiameter;
	float Sx = SurfacePos.X * CoverageFrequency;
	float Sy = SurfacePos.Y * CoverageFrequency;
	float Sz = SurfacePos.Z * CoverageFrequency;

	if (bCoverageUseWarp)
	{
		PlanetNoise::WarpPointFractal(
			Sx, Sy, Sz,
			CoverageWarpStrength,
			/*WarpFreq*/ 1.f,
			CoverageWarpOctaves,
			CoverageLacunarity,
			CoverageGain,
			CovBase,
			CoverageSeed);
	}

	const float CoverageNoise = PlanetNoise::FBm(
		Sx, Sy, Sz,
		CoverageOctaves, CoverageLacunarity, CoverageGain,
		CovBase, CoverageSeed);

	// Orbis: lerp(Min, Max, noise*0.5+0.5) then saturate(sqrt(...)).
	const float CoverageMapped = FMath::Lerp(NoiseOutputMin, NoiseOutputMax, CoverageNoise * 0.5f + 0.5f);
	return FMath::Sqrt(FMath::Clamp(CoverageMapped, 0.f, 1.f));
}

UTextureCube* UPlanetNoiseSpherePreviewComponent::CreateCubeTexture(int32 Dim) const
{
	UTextureCube* Cube = UTextureCube::CreateTransient(Dim, Dim, PF_B8G8R8A8);
	check(Cube);

	Cube->CompressionSettings = TC_EditorIcon;
	Cube->LODGroup = TEXTUREGROUP_Pixels2D;
	Cube->SRGB = true;
	Cube->Filter = TF_Bilinear;
	Cube->NeverStream = true;
	Cube->MipGenSettings = TMGS_NoMipmaps;
	Cube->UpdateResource();
	return Cube;
}

UTexture2D* UPlanetNoiseSpherePreviewComponent::CreateFaceTexture(int32 Dim) const
{
	UTexture2D* Texture = UTexture2D::CreateTransient(Dim, Dim, PF_B8G8R8A8);
	check(Texture);

	Texture->CompressionSettings = TC_EditorIcon;
	Texture->LODGroup = TEXTUREGROUP_Pixels2D;
	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->NeverStream = true;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->UpdateResource();
	return Texture;
}

void UPlanetNoiseSpherePreviewComponent::EnsureTextures(int32 Dim)
{
	const bool bCubeOk = PreviewCube
		&& PreviewCube->GetPlatformData()
		&& PreviewCube->GetPlatformData()->SizeX == Dim
		&& PreviewCube->GetPlatformData()->SizeY == Dim;
	const bool bFaceOk = ImGuiFaceTexture
		&& ImGuiFaceTexture->GetSizeX() == Dim
		&& ImGuiFaceTexture->GetSizeY() == Dim;

	if (bCubeOk && bFaceOk)
	{
		return;
	}

	ReleaseImGuiTexture();

	if (!bCubeOk)
	{
		PreviewCube = CreateCubeTexture(Dim);
	}
	if (!bFaceOk)
	{
		ImGuiFaceTexture = CreateFaceTexture(Dim);
	}
}

void UPlanetNoiseSpherePreviewComponent::WriteCubePixels(const TArray<FColor>& PixelsAllFaces)
{
	if (!PreviewCube || !PreviewCube->GetPlatformData() || PreviewCube->GetPlatformData()->Mips.Num() == 0)
	{
		return;
	}

	FTexture2DMipMap& Mip = PreviewCube->GetPlatformData()->Mips[0];
	const int64 ExpectedBytes = static_cast<int64>(PixelsAllFaces.Num()) * sizeof(FColor);
	if (Mip.BulkData.GetBulkDataSize() < ExpectedBytes)
	{
		return;
	}

	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (!Data)
	{
		return;
	}
	FMemory::Memcpy(Data, PixelsAllFaces.GetData(), static_cast<SIZE_T>(ExpectedBytes));
	Mip.BulkData.Unlock();
	PreviewCube->UpdateResource();
}

void UPlanetNoiseSpherePreviewComponent::WriteFaceTexture(const TArray<FColor>& FacePixels)
{
	if (!ImGuiFaceTexture || !ImGuiFaceTexture->GetPlatformData() || ImGuiFaceTexture->GetPlatformData()->Mips.Num() == 0)
	{
		return;
	}

	FTexture2DMipMap& Mip = ImGuiFaceTexture->GetPlatformData()->Mips[0];
	const int64 ExpectedBytes = static_cast<int64>(FacePixels.Num()) * sizeof(FColor);
	if (Mip.BulkData.GetBulkDataSize() < ExpectedBytes)
	{
		return;
	}

	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (!Data)
	{
		return;
	}
	FMemory::Memcpy(Data, FacePixels.GetData(), static_cast<SIZE_T>(ExpectedBytes));
	Mip.BulkData.Unlock();
	ImGuiFaceTexture->UpdateResource();
}

void UPlanetNoiseSpherePreviewComponent::RegeneratePreview()
{
	const int32 Dim = FMath::Clamp(Resolution, 16, MaxResolution);
	Resolution = Dim;

	EnsureTextures(Dim);
	if (!PreviewCube || !PreviewCube->GetPlatformData() || PreviewCube->GetPlatformData()->Mips.Num() == 0)
	{
		return;
	}

	const int32 FaceCount = 6;
	const int32 FaceTexels = Dim * Dim;
	TArray<FColor> AllFaces;
	AllFaces.SetNumUninitialized(FaceCount * FaceTexels);

	const float InvDim = 1.f / static_cast<float>(Dim);
	const float Radius = OuterRadius;
	const int32 FaceForImGui = FMath::Clamp(ImGuiFaceIndex, 0, 5);
	const double T0 = FPlatformTime::Seconds();

	ParallelFor(FaceCount * Dim, [this, Dim, InvDim, Radius, FaceForImGui, &AllFaces](int32 FlatY)
	{
		const int32 Face = FlatY / Dim;
		const int32 Y = FlatY % Dim;
		FColor* Row = AllFaces.GetData() + Face * Dim * Dim + Y * Dim;
		const float FaceV = (static_cast<float>(Y) + 0.5f) * InvDim * 2.f - 1.f;

		for (int32 X = 0; X < Dim; ++X)
		{
			const float FaceU = (static_cast<float>(X) + 0.5f) * InvDim * 2.f - 1.f;
			const FVector3f Dir = GetCubeDirection(Face, FaceU, FaceV);
			const FVector3f SurfacePos = Dir * Radius;

			const float Coverage = SampleCoverage(SurfacePos);
			const uint8 G = static_cast<uint8>(FMath::Clamp(Coverage, 0.f, 1.f) * 255.f + 0.5f);
			Row[X] = FColor(G, G, G, 255);
		}
	});

	LastFillMs = static_cast<float>((FPlatformTime::Seconds() - T0) * 1000.0);
	LastSampleCount = FaceCount * FaceTexels;

	WriteCubePixels(AllFaces);

	TArray<FColor> FacePixels;
	FacePixels.SetNumUninitialized(FaceTexels);
	FMemory::Memcpy(
		FacePixels.GetData(),
		AllFaces.GetData() + FaceForImGui * FaceTexels,
		static_cast<SIZE_T>(FaceTexels) * sizeof(FColor));
	WriteFaceTexture(FacePixels);
	RegisterImGuiTexture();

	if (IsValid(PreviewCube))
	{
		OnPreviewReady.Broadcast(PreviewCube);
	}
}

void UPlanetNoiseSpherePreviewComponent::RegisterImGuiTexture()
{
#ifdef IMGUI_API
	if (!ImGuiFaceTexture)
	{
		return;
	}
	ImGuiTextureName = FName(*FString::Printf(TEXT("PlanetNoiseSpherePreview_%p"), this));
	ReleaseImGuiTexture();
	FImGuiModule::Get().RegisterTexture(ImGuiTextureName, ImGuiFaceTexture, false);
	bImGuiTextureRegistered = true;
#endif
}

void UPlanetNoiseSpherePreviewComponent::ReleaseImGuiTexture()
{
#ifdef IMGUI_API
	if (bImGuiTextureRegistered)
	{
		FImGuiTextureHandle Handle = FImGuiModule::Get().FindTextureHandle(ImGuiTextureName);
		if (!Handle.IsNull())
		{
			FImGuiModule::Get().ReleaseTexture(Handle);
		}
		bImGuiTextureRegistered = false;
	}
#endif
}

void UPlanetNoiseSpherePreviewComponent::ResetNoiseParamsToDefaults()
{
	Resolution = 256;
	OuterRadius = 50.f;
	CoverageNoiseScale = 4.f;
	CoverageSeed = 1337;
	CoverageBase = EPlanetNoiseBaseType::Value;
	NoiseOutputMin = -1.f;
	NoiseOutputMax = 1.f;
	CoverageOctaves = 8;
	CoverageLacunarity = 2.f;
	CoverageGain = 0.5f;
	bCoverageUseWarp = true;
	CoverageWarpStrength = 0.5f;
	CoverageWarpOctaves = 6;
	ImGuiFaceIndex = 4;
}

void UPlanetNoiseSpherePreviewComponent::DrawImGui()
{
#ifdef IMGUI_API
	ImGui::SetNextWindowSize(ImVec2(420.f, 820.f), ImGuiCond_FirstUseEver);
	const FString WindowTitle = FString::Printf(TEXT("Planet Noise Sphere##%s"), *GetName());
	if (ImGui::Begin(TCHAR_TO_UTF8(*WindowTitle)))
	{
		ImGui::Text("%d^2 x 6 | %d samples | %.3f ms", Resolution, LastSampleCount, LastFillMs);

		bool bParamsChanged = false;

		ImGui::Separator();
		ImGui::TextUnformatted("Output");
		bParamsChanged |= ImGui::DragInt("Resolution", &Resolution, 1, 16, MaxResolution, "%d");
		ImGui::SameLine(); ImGui::TextDisabled("(%d)", 256);
		bParamsChanged |= ImGui::DragFloat("Outer Radius", &OuterRadius, 1.f, 1.f, 1.e9f, "%.1f");
		ImGui::SameLine(); ImGui::TextDisabled("(%.1f)", 50.f);

		ImGui::Separator();
		ImGui::TextUnformatted("Coverage");
		bParamsChanged |= ImGui::DragFloat("Coverage Scale", &CoverageNoiseScale, 0.05f, 0.1f, 2048.f, "%.3f");
		ImGui::SameLine(); ImGui::TextDisabled("(%.3f)", 4.f);
		bParamsChanged |= ImGui::DragInt("Coverage Seed", &CoverageSeed, 1.f, 0, 0, "%d");
		ImGui::SameLine(); ImGui::TextDisabled("(%d)", 1337);
		{
			int32 BaseIdx = 0;
			if (CoverageBase == EPlanetNoiseBaseType::Perlin) BaseIdx = 1;
			else if (CoverageBase == EPlanetNoiseBaseType::Value) BaseIdx = 2;
			if (ImGui::RadioButton("Cov Simplex", BaseIdx == 0)) { CoverageBase = EPlanetNoiseBaseType::Simplex; bParamsChanged = true; }
			ImGui::SameLine();
			if (ImGui::RadioButton("Cov Perlin", BaseIdx == 1)) { CoverageBase = EPlanetNoiseBaseType::Perlin; bParamsChanged = true; }
			ImGui::SameLine();
			if (ImGui::RadioButton("Cov Value", BaseIdx == 2)) { CoverageBase = EPlanetNoiseBaseType::Value; bParamsChanged = true; }
		}
		bParamsChanged |= ImGui::DragFloat("Output Min", &NoiseOutputMin, 0.01f, -1.f, 1.f, "%.3f");
		bParamsChanged |= ImGui::DragFloat("Output Max", &NoiseOutputMax, 0.01f, -1.f, 1.f, "%.3f");
		bParamsChanged |= ImGui::InputInt("Coverage Octaves", &CoverageOctaves);
		CoverageOctaves = FMath::Clamp(CoverageOctaves, 1, 16);
		bParamsChanged |= ImGui::DragFloat("Coverage Lac", &CoverageLacunarity, 0.05f, 1.f, 4.f, "%.3f");
		bParamsChanged |= ImGui::DragFloat("Coverage Gain", &CoverageGain, 0.01f, 0.05f, 0.95f, "%.3f");
		bParamsChanged |= ImGui::Checkbox("Coverage Warp", &bCoverageUseWarp);
		if (bCoverageUseWarp)
		{
			bParamsChanged |= ImGui::DragFloat("Warp Strength", &CoverageWarpStrength, 0.01f, 0.f, 8.f, "%.3f");
			bParamsChanged |= ImGui::InputInt("Warp Octaves", &CoverageWarpOctaves);
			CoverageWarpOctaves = FMath::Clamp(CoverageWarpOctaves, 1, 16);
		}

		ImGui::Separator();
		bParamsChanged |= ImGui::SliderInt("ImGui Face", &ImGuiFaceIndex, 0, 5);
		ImGui::SameLine(); ImGui::TextDisabled("(+Z=4)");

		if (ImGui::Button("Draw", ImVec2(120.f, 0.f)))
		{
			bParamsChanged = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset", ImVec2(120.f, 0.f)))
		{
			ResetNoiseParamsToDefaults();
			bParamsChanged = true;
		}

		if (bParamsChanged)
		{
			RegeneratePreview();
		}

		const float PreviewSide = FMath::Max(256.f, ImGui::GetContentRegionAvail().x);
		if (bImGuiTextureRegistered)
		{
			FImGuiTextureHandle Handle = FImGuiModule::Get().FindTextureHandle(ImGuiTextureName);
			if (!Handle.IsNull())
			{
				ImGui::Image(Handle, ImVec2(PreviewSide, PreviewSide));
			}
			else
			{
				ImGui::Dummy(ImVec2(PreviewSide, PreviewSide));
			}
		}
		else
		{
			ImGui::Dummy(ImVec2(PreviewSide, PreviewSide));
		}
	}
	ImGui::End();
#endif
}
