#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Noise/PlanetNoise.h"
#include "PlanetNoisePreviewComponent.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EPlanetNoiseBaseType : uint8
{
	Simplex,
	Perlin,
	Value
};

UENUM(BlueprintType)
enum class EPlanetWarpStyle : uint8
{
	/** Progressive fractal warp (1 octave = old WarpPoint). */
	Progressive,
	/** Quilez: fbm(p + Amp * fbm(p)). */
	Quilez,
	/** Quilez nested: fbm(p + fbm(p + fbm(p))). */
	QuilezNested
};

/** Fired after noise is generated and PreviewTexture is filled (same or new UTexture2D*). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanetNoisePreviewReady, UTexture2D*, Texture);

/**
 * Multithreaded CPU noise → PreviewTexture.
 * ImGui: pick Simplex/Perlin, toggle FBm + Warp (params per layer), Draw.
 */
UCLASS(ClassGroup = (Planet), meta = (BlueprintSpawnableComponent))
class DELAUNATORPLUGIN_API UPlanetNoisePreviewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlanetNoisePreviewComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Planet Noise")
	void RegeneratePreview();

	/** Restore ImGui/noise params to constructor defaults (does not Draw). */
	UFUNCTION(BlueprintCallable, Category = "Planet Noise")
	void ResetNoiseParamsToDefaults();

	/** Bind in BP/C++: set MID texture param from Texture (re-bind when Size recreates the asset). */
	UPROPERTY(BlueprintAssignable, Category = "Planet Noise")
	FOnPlanetNoisePreviewReady OnPreviewReady;

	// --- Output ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise", meta = (ClampMin = "16", ClampMax = "8192"))
	int32 Size = 512;

	/** Pan in noise space: UV 0..1 maps to [Offset, Offset + Scale]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise")
	float OffsetX = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise")
	float OffsetY = 0.f;

	/** Frequency / window size: UV 0..1 spans this many noise units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise", meta = (ClampMin = "0.001"))
	float Scale = 4.f;

	// --- Base ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|Base")
	EPlanetNoiseBaseType BaseType = EPlanetNoiseBaseType::Simplex;

	// --- FBm layer ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|FBm")
	bool bEnableFBm = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|FBm", meta = (ClampMin = "1", ClampMax = "16", EditCondition = "bEnableFBm"))
	int32 FBmOctaves = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|FBm", meta = (EditCondition = "bEnableFBm"))
	float FBmLacunarity = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|FBm", meta = (EditCondition = "bEnableFBm"))
	float FBmGain = 0.5f;

	// --- Warp layer ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|Warp")
	bool bEnableWarp = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|Warp", meta = (EditCondition = "bEnableWarp"))
	EPlanetWarpStyle WarpStyle = EPlanetWarpStyle::Progressive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|Warp", meta = (EditCondition = "bEnableWarp"))
	float WarpAmplitude = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|Warp", meta = (EditCondition = "bEnableWarp"))
	float WarpFrequency = 1.f;

	/** Progressive only. 1 = single displace (previous behavior). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|Warp", meta = (ClampMin = "1", ClampMax = "16", EditCondition = "bEnableWarp"))
	int32 WarpOctaves = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|Warp", meta = (EditCondition = "bEnableWarp"))
	float WarpLacunarity = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise|Warp", meta = (EditCondition = "bEnableWarp"))
	float WarpGain = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise")
	bool bShowImGui = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Noise")
	TObjectPtr<UTexture2D> PreviewTexture;

	/** Same texels as PreviewTexture, SRGB=false for ImGui/Slate (display-referred). */
	UPROPERTY()
	TObjectPtr<UTexture2D> ImGuiDisplayTexture;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Noise")
	float LastFillMs = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Noise")
	int32 LastSampleCount = 0;

	static constexpr int32 MaxSize = 8192;

private:
	void EnsureTexture(int32 Dim);
	UTexture2D* CreatePreviewTexture(int32 Dim, bool bSRGB) const;
	void WriteTexturePixels(UTexture2D* Texture, const TArray<FColor>& Pixels);
	void RegisterImGuiTexture();
	void ReleaseImGuiTexture();
	void DrawImGui();

	/** Pipeline: optional WarpPoint → FBm or single Sample. */
	float SampleNoise(float X, float Y) const;

	PlanetNoise::EBase GetBase() const
	{
		switch (BaseType)
		{
		case EPlanetNoiseBaseType::Perlin: return PlanetNoise::EBase::Perlin;
		case EPlanetNoiseBaseType::Value:  return PlanetNoise::EBase::Value;
		case EPlanetNoiseBaseType::Simplex:
		default:                           return PlanetNoise::EBase::Simplex;
		}
	}

	FName ImGuiTextureName;
	bool bImGuiTextureRegistered = false;
	uint64 LastImGuiDrawFrame = MAX_uint64;
};
