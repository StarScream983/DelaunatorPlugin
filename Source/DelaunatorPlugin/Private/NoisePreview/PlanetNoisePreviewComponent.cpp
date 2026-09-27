#include "NoisePreview/PlanetNoisePreviewComponent.h"
#include "Async/ParallelFor.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"

#ifdef IMGUI_API
#include "ImGuiModule.h"
#include "ImGuiTextureHandle.h"
#include <imgui.h>
#endif

UPlanetNoisePreviewComponent::UPlanetNoisePreviewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Editor tick + PIE tick both submitted the same ImGui window (controls appeared twice).
	bTickInEditor = false;
}

void UPlanetNoisePreviewComponent::BeginPlay()
{
	Super::BeginPlay();

	// Next tick so actor Blueprints can bind OnPreviewReady in BeginPlay first.
	// (Component BeginPlay often runs before the actor graph binds the delegate.)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UPlanetNoisePreviewComponent::RegeneratePreview);
	}
}

void UPlanetNoisePreviewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseImGuiTexture();
	Super::EndPlay(EndPlayReason);
}

void UPlanetNoisePreviewComponent::TickComponent(
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

	// One ImGui submit per component per frame (guards double-tick edge cases).
	if (LastImGuiDrawFrame == GFrameCounter)
	{
		return;
	}
	LastImGuiDrawFrame = GFrameCounter;

	DrawImGui();
}

float UPlanetNoisePreviewComponent::SampleNoise(float X, float Y) const
{
	float Sx = X;
	float Sy = Y;
	float Sz = 0.f;
	const PlanetNoise::EBase Base = GetBase();

	if (bEnableWarp)
	{
		if (WarpStyle == EPlanetWarpStyle::Quilez)
		{
			// Quilez pattern is a full fBm(p + fBm(p)); ignore separate FBm toggle.
			const int32 Oct = bEnableFBm ? FBmOctaves : 1;
			const float Lac = bEnableFBm ? FBmLacunarity : 2.f;
			const float Gn = bEnableFBm ? FBmGain : 0.5f;
			return PlanetNoise::PatternQuilez(Sx, Sy, Sz, WarpAmplitude, Oct, Lac, Gn, Base, Seed);
		}
		if (WarpStyle == EPlanetWarpStyle::QuilezNested)
		{
			const int32 Oct = bEnableFBm ? FBmOctaves : 1;
			const float Lac = bEnableFBm ? FBmLacunarity : 2.f;
			const float Gn = bEnableFBm ? FBmGain : 0.5f;
			return PlanetNoise::PatternQuilezNested(Sx, Sy, Sz, WarpAmplitude, Oct, Lac, Gn, Base, Seed);
		}

		PlanetNoise::WarpPointFractal(
			Sx, Sy, Sz,
			WarpAmplitude, WarpFrequency,
			WarpOctaves, WarpLacunarity, WarpGain,
			Base, Seed);
	}

	if (bEnableFBm)
	{
		return PlanetNoise::FBm(Sx, Sy, Sz, FBmOctaves, FBmLacunarity, FBmGain, Base, Seed);
	}

	return PlanetNoise::Sample(Base, Sx, Sy, Sz, Seed);
}

UTexture2D* UPlanetNoisePreviewComponent::CreatePreviewTexture(int32 Dim, bool bSRGB) const
{
	UTexture2D* Texture = UTexture2D::CreateTransient(Dim, Dim, PF_B8G8R8A8);
	check(Texture);

	// Flags that play nice with material samplers (not displacement/normal compression).
	Texture->CompressionSettings = TC_EditorIcon;
	Texture->LODGroup = TEXTUREGROUP_Pixels2D;
	Texture->SRGB = bSRGB;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->NeverStream = true;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->UpdateResource();
	return Texture;
}

void UPlanetNoisePreviewComponent::EnsureTexture(int32 Dim)
{
	const bool bPreviewOk = PreviewTexture && PreviewTexture->GetSizeX() == Dim && PreviewTexture->GetSizeY() == Dim;
	const bool bImGuiOk = ImGuiDisplayTexture && ImGuiDisplayTexture->GetSizeX() == Dim && ImGuiDisplayTexture->GetSizeY() == Dim;
	if (bPreviewOk && bImGuiOk)
	{
		return;
	}

	ReleaseImGuiTexture();

	if (!bPreviewOk)
	{
		PreviewTexture = CreatePreviewTexture(Dim, true);
	}
	if (!bImGuiOk)
	{
		// Slate samples UTexture RHI directly. An sRGB texture is decoded to linear and looks darker
		// than the mesh; keep display-referred bytes (SRGB=false) for ImGui only.
		ImGuiDisplayTexture = CreatePreviewTexture(Dim, false);
	}
}

void UPlanetNoisePreviewComponent::WriteTexturePixels(UTexture2D* Texture, const TArray<FColor>& Pixels)
{
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return;
	}

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	const int64 ExpectedBytes = static_cast<int64>(Pixels.Num()) * sizeof(FColor);
	if (Mip.BulkData.GetBulkDataSize() < ExpectedBytes)
	{
		return;
	}

	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (!Data)
	{
		return;
	}
	FMemory::Memcpy(Data, Pixels.GetData(), static_cast<SIZE_T>(ExpectedBytes));
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
}

void UPlanetNoisePreviewComponent::RegeneratePreview()
{
	const int32 Dim = FMath::Clamp(Size, 16, MaxSize);
	Size = Dim;

	EnsureTexture(Dim);
	if (!PreviewTexture || !PreviewTexture->GetPlatformData() || PreviewTexture->GetPlatformData()->Mips.Num() == 0)
	{
		return;
	}

	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Dim * Dim);

	const float InvDim = 1.f / static_cast<float>(Dim);
	const float Ox = OffsetX;
	const float Oy = OffsetY;
	const float Sc = Scale;
	const double T0 = FPlatformTime::Seconds();

	ParallelFor(Dim, [this, Dim, InvDim, Ox, Oy, Sc, &Pixels](int32 Y)
	{
		FColor* Row = Pixels.GetData() + Y * Dim;
		const float Ny = Oy + (static_cast<float>(Y) + 0.5f) * InvDim * Sc;

		for (int32 X = 0; X < Dim; ++X)
		{
			const float Nx = Ox + (static_cast<float>(X) + 0.5f) * InvDim * Sc;
			const float N = SampleNoise(Nx, Ny);
			const float N01 = FMath::Clamp(N * 0.5f + 0.5f, 0.f, 1.f);
			const uint8 G = static_cast<uint8>(N01 * 255.f + 0.5f);
			Row[X] = FColor(G, G, G, 255);
		}
	});

	LastFillMs = static_cast<float>((FPlatformTime::Seconds() - T0) * 1000.0);
	LastSampleCount = Dim * Dim;

	WriteTexturePixels(PreviewTexture, Pixels);
	WriteTexturePixels(ImGuiDisplayTexture, Pixels);

	RegisterImGuiTexture();

	if (IsValid(PreviewTexture))
	{
		OnPreviewReady.Broadcast(PreviewTexture);
	}

	// UE_LOG(LogTemp, Log, TEXT("PlanetNoisePreview: %dx%d (%d samples) in %.3f ms  base=%d fbm=%d warp=%d"),
	// 	Dim, Dim, LastSampleCount, LastFillMs,
	// 	static_cast<int32>(BaseType), bEnableFBm ? 1 : 0, bEnableWarp ? 1 : 0);
}

void UPlanetNoisePreviewComponent::RegisterImGuiTexture()
{
#ifdef IMGUI_API
	if (!ImGuiDisplayTexture)
	{
		return;
	}
	ImGuiTextureName = FName(*FString::Printf(TEXT("PlanetNoisePreview_%p"), this));
	ReleaseImGuiTexture();
	FImGuiModule::Get().RegisterTexture(ImGuiTextureName, ImGuiDisplayTexture, false);
	bImGuiTextureRegistered = true;
#endif
}

void UPlanetNoisePreviewComponent::ReleaseImGuiTexture()
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

void UPlanetNoisePreviewComponent::ResetNoiseParamsToDefaults()
{
	Seed = 1337;
	Size = 512;
	OffsetX = 0.f;
	OffsetY = 0.f;
	Scale = 4.f;
	BaseType = EPlanetNoiseBaseType::Simplex;
	bEnableFBm = true;
	FBmOctaves = 5;
	FBmLacunarity = 2.f;
	FBmGain = 0.5f;
	bEnableWarp = false;
	WarpStyle = EPlanetWarpStyle::Progressive;
	WarpAmplitude = 0.5f;
	WarpFrequency = 1.f;
	WarpOctaves = 1;
	WarpLacunarity = 2.f;
	WarpGain = 0.5f;
}

void UPlanetNoisePreviewComponent::DrawImGui()
{
#ifdef IMGUI_API
	// Stable id (not object pointer) so PIE does not create a new tiny FirstUseEver window every run.
	ImGui::SetNextWindowSize(ImVec2(420.f, 780.f), ImGuiCond_FirstUseEver);
	const FString WindowTitle = FString::Printf(TEXT("Planet Noise Preview##%s"), *GetName());
	if (ImGui::Begin(TCHAR_TO_UTF8(*WindowTitle)))
	{
		ImGui::Text("%d x %d | %d samples | %.3f ms", Size, Size, LastSampleCount, LastFillMs);

		bool bParamsChanged = false;

		// --- Output ---
		ImGui::Separator();
		ImGui::TextUnformatted("Output");
		bParamsChanged |= ImGui::DragInt("Size", &Size, 1, 16, MaxSize, "%d");
		ImGui::SameLine(); ImGui::TextDisabled("(%d)", 512);
		bParamsChanged |= ImGui::DragFloat("Offset X", &OffsetX, 0.05f, 0.f, 0.f, "%.3f");
		ImGui::SameLine(); ImGui::TextDisabled("(%.3f)", 0.f);
		bParamsChanged |= ImGui::DragFloat("Offset Y", &OffsetY, 0.05f, 0.f, 0.f, "%.3f");
		ImGui::SameLine(); ImGui::TextDisabled("(%.3f)", 0.f);
		bParamsChanged |= ImGui::DragFloat("Scale", &Scale, 0.05f, 0.001f, 256.f, "%.3f");
		ImGui::SameLine(); ImGui::TextDisabled("(%.3f)", 4.f);
		bParamsChanged |= ImGui::DragInt("Seed", &Seed, 1.f, 0, 0, "%d");
		ImGui::SameLine(); ImGui::TextDisabled("(%d)", 1337);

		// --- Base ---
		ImGui::Separator();
		ImGui::TextUnformatted("Base");
		ImGui::SameLine(); ImGui::TextDisabled("(Simplex)");
		{
			const int32 BaseIdx = static_cast<int32>(BaseType);
			const auto PickBase = [&](int32 Idx, EPlanetNoiseBaseType Type, const char* Label)
			{
				if (ImGui::RadioButton(Label, BaseIdx == Idx))
				{
					BaseType = Type;
					bParamsChanged = true;
				}
			};
			PickBase(0, EPlanetNoiseBaseType::Perlin, "Perlin");
			ImGui::SameLine();
			PickBase(1, EPlanetNoiseBaseType::Simplex, "Simplex");
			ImGui::SameLine();
			PickBase(2, EPlanetNoiseBaseType::Value, "Value");
			PickBase(3, EPlanetNoiseBaseType::OpenSimplex, "OS14");
			ImGui::SameLine();
			PickBase(4, EPlanetNoiseBaseType::OpenSimplex2F, "OS2F");
			ImGui::SameLine();
			PickBase(5, EPlanetNoiseBaseType::OpenSimplex2S, "OS2S");
		}

		// --- FBm layer ---
		ImGui::Separator();
		ImGui::TextUnformatted("FBm");
		bParamsChanged |= ImGui::Checkbox("Enable FBm", &bEnableFBm);
		if (bEnableFBm)
		{
			bParamsChanged |= ImGui::InputInt("Octaves", &FBmOctaves);
			FBmOctaves = FMath::Clamp(FBmOctaves, 1, 16);
			ImGui::SameLine(); ImGui::TextDisabled("(%d)", 5);
			bParamsChanged |= ImGui::DragFloat("Lacunarity", &FBmLacunarity, 0.05f, 1.f, 4.f, "%.3f");
			ImGui::SameLine(); ImGui::TextDisabled("(%.3f)", 2.f);
			bParamsChanged |= ImGui::DragFloat("Gain", &FBmGain, 0.01f, 0.05f, 1.f, "%.3f");
			ImGui::SameLine(); ImGui::TextDisabled("(%.3f)", 0.5f);
		}

		// --- Warp layer ---
		ImGui::Separator();
		ImGui::TextUnformatted("Warp");
		bParamsChanged |= ImGui::Checkbox("Enable Warp", &bEnableWarp);
		if (bEnableWarp)
		{
			const char* StyleLabels[] = { "Progressive", "Quilez", "Quilez Nested" };
			int32 StyleIdx = static_cast<int32>(WarpStyle);
			if (ImGui::Combo("Warp Style", &StyleIdx, StyleLabels, UE_ARRAY_COUNT(StyleLabels)))
			{
				WarpStyle = static_cast<EPlanetWarpStyle>(StyleIdx);
				bParamsChanged = true;
			}
			ImGui::SameLine(); ImGui::TextDisabled("(Progressive)");

			bParamsChanged |= ImGui::DragFloat("Amplitude", &WarpAmplitude, 0.01f, 0.f, 8.f, "%.3f");
			ImGui::SameLine(); ImGui::TextDisabled("(%.3f)", 0.5f);

			if (WarpStyle == EPlanetWarpStyle::Progressive)
			{
				bParamsChanged |= ImGui::DragFloat("Warp Frequency", &WarpFrequency, 0.05f, 0.001f, 16.f, "%.3f");
				ImGui::SameLine(); ImGui::TextDisabled("(%.3f)", 1.f);
				bParamsChanged |= ImGui::InputInt("Warp Octaves", &WarpOctaves);
				WarpOctaves = FMath::Clamp(WarpOctaves, 1, 16);
				ImGui::SameLine(); ImGui::TextDisabled("(%d)", 1);
				bParamsChanged |= ImGui::DragFloat("Warp Lacunarity", &WarpLacunarity, 0.05f, 1.f, 4.f, "%.3f");
				ImGui::SameLine(); ImGui::TextDisabled("(%.3f)", 2.f);
				bParamsChanged |= ImGui::DragFloat("Warp Gain", &WarpGain, 0.01f, 0.05f, 1.f, "%.3f");
				ImGui::SameLine(); ImGui::TextDisabled("(%.3f)", 0.5f);
			}
			else
			{
				ImGui::TextDisabled("Quilez is 2D IQ-style; set Amplitude ~ 4 (article).");
			}
		}

		ImGui::Separator();
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
				// Modeling-Mode plane UVs: flip V (UE V=0 bottom) + swap UV to match stood-up XY quad.
				const ImVec2 UvTL(1.f, 0.f); // swapped + flipped V of (0,1)
				const ImVec2 UvTR(1.f, 1.f); // swapped + flipped V of (1,1)
				const ImVec2 UvBR(0.f, 1.f); // swapped + flipped V of (1,0)
				const ImVec2 UvBL(0.f, 0.f); // swapped + flipped V of (0,0)

				ImDrawList* DrawList = ImGui::GetWindowDrawList();
				const ImVec2 P0 = ImGui::GetCursorScreenPos();
				const ImVec2 P1(P0.x + PreviewSide, P0.y + PreviewSide);
				DrawList->AddImageQuad(Handle, P0, ImVec2(P1.x, P0.y), P1, ImVec2(P0.x, P1.y), UvTL, UvTR, UvBR, UvBL);
				ImGui::Dummy(ImVec2(PreviewSide, PreviewSide));
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
