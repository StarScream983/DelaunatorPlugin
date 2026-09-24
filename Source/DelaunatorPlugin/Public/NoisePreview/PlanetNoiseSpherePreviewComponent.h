#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NoisePreview/PlanetNoisePreviewComponent.h"
#include "PlanetNoiseSpherePreviewComponent.generated.h"

class UTexture2D;
class UTextureCube;

UENUM(BlueprintType)
enum class EPlanetSphereWarpStyle : uint8
{
	/** FastNoise2-style progressive multi-displace. */
	Progressive,
	/** IQ / Orbis: f(p + h(p)) with h from 3 FBms. */
	Iq
};

/** Per-face bake resolution for PreviewCube. */
UENUM(BlueprintType)
enum class EPlanetNoiseCubeResolution : uint8
{
	Res256  UMETA(DisplayName = "256"),
	Res512  UMETA(DisplayName = "512"),
	Res1024 UMETA(DisplayName = "1024"),
	Res2048 UMETA(DisplayName = "2048"),
	Res4096 UMETA(DisplayName = "4096"),
	Res8192 UMETA(DisplayName = "8192"),
};

/** Fired after the coverage cube is filled (re-bind MID when Size recreates the asset). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanetNoiseSpherePreviewReady, UTextureCube*, Texture);

/**
 * Orbis-style Coverage cube bake, but CPU PlanetNoise (no compute shader).
 * Sample: Dir * OuterRadius, Frequency = NoiseScale / (2 * OuterRadius).
 * RGB = Coverage grayscale. Bind PreviewCube to a TextureCube sampler (sample by direction).
 */
UCLASS(ClassGroup = (Planet), meta = (BlueprintSpawnableComponent))
class DELAUNATORPLUGIN_API UPlanetNoiseSpherePreviewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlanetNoiseSpherePreviewComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Planet Noise Sphere")
	void RegeneratePreview();

	UFUNCTION(BlueprintCallable, Category = "Planet Noise Sphere")
	void ResetNoiseParamsToDefaults();

	UPROPERTY(BlueprintAssignable, Category = "Planet Noise Sphere")
	FOnPlanetNoiseSpherePreviewReady OnPreviewReady;

	// --- Output ---
	/** Per-face resolution of the baked cube. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere")
	EPlanetNoiseCubeResolution Resolution = EPlanetNoiseCubeResolution::Res256;

	/** Sphere radius used for Orbis-compatible position (cancels in Frequency * Radius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere", meta = (ClampMin = "1.0"))
	float OuterRadius = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere")
	bool bShowImGui = true;

	// --- Coverage (Orbis Cloud Coverage) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage", meta = (ClampMin = "0.1"))
	float CoverageNoiseScale = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage", meta = (ClampMin = "0"))
	int32 CoverageSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage")
	EPlanetNoiseBaseType CoverageBase = EPlanetNoiseBaseType::Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float NoiseOutputMin = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float NoiseOutputMax = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage", meta = (ClampMin = "1", ClampMax = "16"))
	int32 CoverageOctaves = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float CoverageLacunarity = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float CoverageGain = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage")
	bool bCoverageUseWarp = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage")
	EPlanetSphereWarpStyle CoverageWarpStyle = EPlanetSphereWarpStyle::Iq;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float CoverageWarpStrength = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage", meta = (ClampMin = "1", ClampMax = "16"))
	int32 CoverageWarpOctaves = 6;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Noise Sphere")
	TObjectPtr<UTextureCube> PreviewCube;

	/** One cube face copied for ImGui (display-referred, SRGB=false). */
	UPROPERTY()
	TObjectPtr<UTexture2D> ImGuiFaceTexture;

	/** Which cube face is shown in ImGui (0=+X … 5=-Z). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere", meta = (ClampMin = "0", ClampMax = "5"))
	int32 ImGuiFaceIndex = 4;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Noise Sphere")
	float LastFillMs = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Planet Noise Sphere")
	int32 LastSampleCount = 0;

private:
	static int32 ResolutionToPixels(EPlanetNoiseCubeResolution Res);
	void EnsureTextures(int32 Dim);
	UTextureCube* CreateCubeTexture(int32 Dim) const;
	UTexture2D* CreateFaceTexture(int32 Dim) const;
	void WriteCubePixels(const TArray<FColor>& PixelsAllFaces);
	void WriteFaceTexture(const TArray<FColor>& FacePixels);
	void RegisterImGuiTexture();
	void ReleaseImGuiTexture();
	void DrawImGui();

	FVector3f GetCubeDirection(int32 Face, float FaceU, float FaceV) const;
	PlanetNoise::EBase ToBase(EPlanetNoiseBaseType T) const;
	float SampleCoverage(const FVector3f& SurfacePos) const;

	FName ImGuiTextureName;
	bool bImGuiTextureRegistered = false;
	uint64 LastImGuiDrawFrame = MAX_uint64;
};
