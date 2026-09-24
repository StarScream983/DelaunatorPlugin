#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NoisePreview/PlanetNoisePreviewComponent.h"
#include "PlanetNoiseSpherePreviewComponent.generated.h"

class UTexture2D;
class UTextureCube;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere", meta = (ClampMin = "16", ClampMax = "4096"))
	int32 Resolution = 256;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage", meta = (ClampMin = "0.0", ClampMax = "8.0", EditCondition = "bCoverageUseWarp"))
	float CoverageWarpStrength = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planet Noise Sphere|Coverage", meta = (ClampMin = "1", ClampMax = "16", EditCondition = "bCoverageUseWarp"))
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

	static constexpr int32 MaxResolution = 4096;

private:
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
