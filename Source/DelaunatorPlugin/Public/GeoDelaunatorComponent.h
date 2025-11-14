// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Delaunator.h"
#include <sleef.h>
#include <functional>
#include <array>
#include "GeoDelaunatorComponent.generated.h"


#define _PI UE_DOUBLE_PI
#define _TAU UE_DOUBLE_TWO_PI
#define DEGREES (180. / UE_DOUBLE_PI)
#define RADIANS (UE_DOUBLE_PI / 180.)


struct FRotation
{
	std::function<FVector2D(FVector2D)> forward; // like rotate(?, ?)
	std::function<FVector2D(FVector2D)> invert;  // like rotate.invert(?, ?)
};

// Wrap lambda to [-PI, PI]
static inline double WrapPi(double lambda)
{
	if (FMath::Abs(lambda) > _PI)
		lambda -= FMath::RoundToDouble(lambda / _TAU) * _TAU;
	return lambda;
}

static inline FRotation Compose(const FRotation& a, const FRotation& b)
{
	FRotation c;

	// forward: a ? b
	c.forward = [a, b](FVector2D p) {
		return b.forward(a.forward(p));
		};

	// invert: b^{-1} ? a^{-1}   (only if both inverses exist)
	if (a.invert && b.invert)
	{
		c.invert = [a, b](FVector2D p) {
			/*first undo 'a', then undo 'b'’s input mapping order from JS:
			x = b.invert(x,y); x && a.invert(x[0], x[1])
			x && ... is a short - circuit check :
			if x is truthy(an array like[?, ?]), call a.invert(x[0], x[1]);
			else skip it and return x(likely null).*/
			FVector2D q = b.invert(p);
			return a.invert ? a.invert(q) : q; // safety; mirrors JS guard
			};
	}
	else
	{
		// leave c.invert empty if either a/b has no invert
		c.invert = {};
	}

	return c;
}

struct FGeoRotation
{

private:
	FRotation RotateR; // radians-domain rotation (forward + invert)

public:
	FGeoRotation() = default;

	// pivot in DEGREES, just like d3.geoRotation(pivot)
	explicit FGeoRotation(FVector2D pivotDeg)
	{
		RotateR = RotateRadians(pivotDeg.X * RADIANS, pivotDeg.Y * RADIANS, 0.0);
	}

	// forward: degrees in → degrees out
	FVector2D Forward(FVector2D coordinatesDeg) const
	{
		FVector2D rad = coordinatesDeg * RADIANS;
		FVector2D out = RotateR.forward ? RotateR.forward(rad) : rad;
		return out * DEGREES;
	}

	// invert: degrees in → degrees out (this is what we'll actually use)
	FVector2D Invert(FVector2D coordinatesDeg) const
	{
		FVector2D rad = coordinatesDeg * RADIANS;
		FVector2D out = RotateR.invert ? RotateR.invert(rad) : rad;
		return out * DEGREES;
	}

	static FRotation RotationIdentity() {
		FRotation r;
		r.forward = [](FVector2D p) { return FVector2D(WrapPi(p.X), p.Y); };
		r.invert = r.forward;
		return r;
	}

	static FRotation RotateRadians(double deltaLambda, double deltaPhi, double deltaGamma = 0.0)
	{
		deltaLambda = FMath::Fmod(deltaLambda, _TAU);

		if (deltaLambda != 0.0)
		{
			if (deltaPhi != 0.0 || deltaGamma != 0.0)
				return Compose(RotationLambda(-deltaLambda), RotationPhiGamma(deltaPhi, deltaGamma));
			else
				return RotationLambda(-deltaLambda);
		}
		else
		{
			if (deltaPhi != 0.0 || deltaGamma != 0.0)
				return RotationPhiGamma(deltaPhi, deltaGamma);
			else
				return RotationIdentity();
		}
	}

	static FRotation RotationLambda(double deltaLambda) {
		FRotation r;
		// lambda = p.X, phi = p.Y
		r.forward = [=](FVector2D p) { return FVector2D(WrapPi(p.X + deltaLambda), p.Y); };
		r.invert = [=](FVector2D p) { return FVector2D(WrapPi(p.X - deltaLambda), p.Y); };
		return r;
	}

	static FRotation RotationPhiGamma(double deltaPhi, double deltaGamma = 0.0)
	{
		// Precompute once (radians)
		const double cosDeltaPhi = Sleef_cos_u10(deltaPhi);
		const double sinDeltaPhi = Sleef_sin_u10(deltaPhi);
		const double cosDeltaGamma = Sleef_cos_u10(deltaGamma); // replace by 1.0
		const double sinDeltaGamma = Sleef_sin_u10(deltaGamma); // replace by 0.0

		FRotation rPhiGamma;

		// forward
		rPhiGamma.forward = [=](FVector2D coordinates)
			{
				const double lambda = coordinates.X;
				const double phi = coordinates.Y;

				const double cosPhi = Sleef_cos_u10(phi);
				const double x = Sleef_cos_u10(lambda) * cosPhi;
				const double y = Sleef_sin_u10(lambda) * cosPhi;
				const double z = Sleef_sin_u10(phi);

				const double k = z * cosDeltaPhi + x * sinDeltaPhi;

				const double lambdaOut = Sleef_atan2_u10(
					y * cosDeltaGamma - k * sinDeltaGamma,
					x * cosDeltaPhi - z * sinDeltaPhi
				);
				const double phiOut = Sleef_asin_u10(k * cosDeltaGamma + y * sinDeltaGamma); // SClampUnit -1.0, 1.0

				return FVector2D(lambdaOut, phiOut);
			};

		// invert
		rPhiGamma.invert = [=](FVector2D coordinates)
			{
				const double lambda = coordinates.X;
				const double phi = coordinates.Y;

				const double cosPhi = Sleef_cos_u10(phi);
				const double x = Sleef_cos_u10(lambda) * cosPhi;
				const double y = Sleef_sin_u10(lambda) * cosPhi;
				const double z = Sleef_sin_u10(phi);

				const double k = z * cosDeltaGamma - y * sinDeltaGamma;

				const double lambdaOut = Sleef_atan2_u10(
					y * cosDeltaGamma + z * sinDeltaGamma,
					x * cosDeltaPhi + k * sinDeltaPhi
				);
				const double phiOut = Sleef_asin_u10(k * cosDeltaPhi - x * sinDeltaPhi); // SClampUnit -1.0, 1.0

				return FVector2D(lambdaOut, phiOut);
			};

		return rPhiGamma;
	}
};

struct FGeoMatrixRotation
{
	FGeoMatrixRotation() {}
	// pivot in DEGREES (same as before)
	explicit FGeoMatrixRotation(FVector2D pivotDeg);

	// Keep this so your code compiles, but it’s no longer used by FGeoRotation itself.
	static std::function<FVector2D(FVector2D)> MatrixRotateRadians(double deltaLambda, double deltaPhi, double deltaGamma = 0.);

	// Keep the helpers you already had (RotationLambda / RotationPhiGamma / RotationIdentity)
	static std::function<FVector2D(FVector2D)> MatrixRotationIdentity();
	static std::function<FVector2D(FVector2D)> MatrixRotationLambda(double deltaLambda);
	static std::function<FVector2D(FVector2D)> MatrixRotationPhiGamma(double deltaPhi, double deltaGamma = 0.);

	// d3-geo-style API you’re calling: returns DEGREES
	FVector2D Invert(FVector2D coordinatesDeg) const;

	// Kept for compatibility (not used internally anymore)
	std::function<FVector2D(FVector2D)> Rotate;

private:
	// 3x3 rotation matrix that maps pivot -> south pole
	double M[3][3];   // forward  (apply to go pivot->south frame)
	double MT[3][3];  // inverse  (transpose)

	static void BuildRotationBetweenUnitVectors(const double a[3], const double b[3], double R[3][3]);
	static void Transpose3(const double A[3][3], double AT[3][3]);
	static void MulVec3(const double A[3][3], const double v[3], double out[3]);
};

struct FGeoStereographic
{
//public:
	FGeoStereographic() {
		_project = StereoGraphicRaw();
	};

	double _k{150.}, // scale 150
		_x{ 480. } /*480.*/, _y{ 250. } /*250*/, // translate
		_lambda{ 0. } /*0.*/, _phi{ 0. } /*0.*/, // center

		// pre-rotate
		_deltaLambda{0.} /*0.*/, _deltaPhi{ 0. } /*0.*/, _deltaGamma{ 0. } /*0.*/;
	std::function<FVector2D(FVector2D)> _rotate;

	double _alpha{ 0. } /*0.*/, // post-rotate angle
		_sx{1.}, _sy{ 1. }; // reflectX, reflectY, both 1.

	std::function<FVector2D(FVector2D)> _project;

	std::function<FVector2D(FVector2D)> _projectTransform;
	std::function<FVector2D(FVector2D)> _projectRotateTransform;

	std::function<FVector2D(FVector2D)> _projection() {
		Recenter();
		return _projectRotateTransform;
	}

	std::function<FVector2D(FVector2D)> _projection(std::function<FVector2D(FVector2D)> inProject) {
		_project = inProject;
		Recenter();
		return _projectRotateTransform;
	}

	FGeoStereographic& Translate(FVector2D inTranslate) {
		_x = inTranslate.X;
		_y = inTranslate.Y;
		Recenter();
		return *this;
	}

	FGeoStereographic& Scale(double inScale) {
		_k = inScale;
		Recenter();
		return *this;
	}

	FGeoStereographic& Rotate(FVector2D inDelta) {
		_deltaLambda = std::fmod(inDelta.X, 360.0) * RADIANS;
		_deltaPhi = std::fmod(inDelta.Y, 360.0) * RADIANS;
		Recenter();
		return *this;
	}

	FGeoStereographic& Rotate(FVector inDelta) {
		_deltaLambda = std::fmod(inDelta.X, 360.0) * RADIANS;
		_deltaPhi = std::fmod(inDelta.Y, 360.0) * RADIANS;
		_deltaGamma = std::fmod(inDelta.Z, 360.0) * RADIANS;
		Recenter();
		return *this;
	}

	/*FGeoStereographic* ClipAngle() {
		return this;
	}*/

	std::function<FVector2D(FVector2D)> StereoGraphicRaw()
	{
		return [](FVector2D point) {
			double cy = Sleef_cos_u10(point.Y), k = 1 + Sleef_cos_u10(point.X) * cy;
			return FVector2D(cy * Sleef_sin_u10(point.X) / k, Sleef_sin_u10(point.Y) / k);
			};
	}

	std::function<FVector2D(FVector2D)> scaleTranslate(double k, double dx, double dy, double sx, double sy)
	{
		return [=](FVector2D point) {
			point.X *= sx; point.Y *= sy;
			return FVector2D((point.X - dx) / k * sx, (dy - point.Y) / k * sy);
			};
	}

	std::function<FVector2D(FVector2D)> scaleTranslateRotate(double k, double dx, double dy, double sx, double sy, double alpha)
	{
		if (alpha == 0.) return scaleTranslate(k, dx, dy, sx, sy);
		double cosAlpha = Sleef_cos_u10(alpha),
			sinAlpha = Sleef_sin_u10(alpha),
			a = cosAlpha * k,
			b = sinAlpha * k,
			ai = cosAlpha / k,
			bi = sinAlpha / k,
			ci = (sinAlpha * dy - cosAlpha * dx) / k,
			fi = (sinAlpha * dx + cosAlpha * dy) / k;
		return [=](FVector2D point) {
			point.X *= sx; point.Y *= sy;
			return FVector2D(a * point.X - b * point.Y + dx, dy - b * point.X - a * point.Y);
			};
	}

	void Recenter() {
		FVector2D center = scaleTranslateRotate(_k, 0, 0, _sx, _sy, _alpha)(_project(FVector2D(_lambda, _phi)));
		/*FVector2D projected = _project(FVector2D(_lambda, _phi));
		auto transformFunc = scaleTranslateRotate(_k, 0, 0, _sx, _sy, _alpha);
		FVector2D center = transformFunc(projected);*/

		/*std::function<FVector2D(FVector2D)> transform = scaleTranslateRotate(_k, _x - center[0], _y - center[1], _sx, _sy, _alpha);
		_rotate = FGeoRotation::RotateRadians(_deltaLambda, _deltaPhi, _deltaGamma);
		_projectTransform = Compose(_project, transform);
		_projectRotateTransform = Compose(_rotate, _projectTransform);*/
	}
};

struct MyStruct {
public:
	int a;
	double b;

	// Define a member function that returns a std::function
	std::function<void()> getFunction() {
		int da = 2;
		double db = 2.1;
		// [=, this] Captures member variables 'a' and 'b' of the struct and da, db in getFunction by value in the lambda
		return [=, this]() {
			// Inside the lambda, you can access 'a' and 'b' directly
			//UE_LOG(LogTemp, Warning, TEXT("a: %d, b: %f"), a+da, b+db);
			};
	}
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DELAUNATORPLUGIN_API UGeoDelaunatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGeoDelaunatorComponent();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	// PSEUDO-RNG
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	int64 RandomSeed{ 2236 };
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Terrain")
	FRandomStream RngStream;

	// FIBONACCI SPHERE
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "100", UIMin = "100"), Category = "GeoDelaunator")
	int32 N = 100; // number of Fibonacci points

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0", UIMin = "1.0"), Category = "GeoDelaunator")
	double PlanetRadius = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GeoDelaunator")
	double Jitter = 9157.;

	// DEBUG DRAW POINTS AND TRIANGLE LINES
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "4.0", UIMin = "4.0", ClampMax = "12.0", UIMax="12.0"), Category = "GeoDelaunator")
	float DebugPointScale = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1.0", UIMin = "1.0", ClampMax = "5.0", UIMax = "5.0"), Category = "GeoDelaunator")
	float DebugLineThickness = 1.f;

	UPROPERTY()
	UDelaunator* Delaunator = nullptr;

	TArray<FVector2D> LonLat;
	TArray<FVector> FibonacciPoints;
	std::vector<double> coords; // FOR DELAUNAY
	TArray<FIntVector> SphericalTriangles;

	TArray<FVector2D> Projected2D; // FibonacciPoints after Stereographic Projection
	TArray<int32> IndexMap; // map from ProjectedPoints to LonLat, dunno if needed

	//STICHING INTERPOLATION FOR TESTING
	TArray<FVector> PivotedPoints;

	// INTERPOLATION DEBUG
	float InterpolationT = 0.f;
	FQuat PivotToSouthQuat = FQuat::Identity;
	FTimerHandle THandle_Interpolate;
	void Timer_FibonacciInterpolation();

public:
	
	void GenerateFibonacciSphere1();
	void GenerateFibonacciSphere2();
	void GeoRotation(int32 PivotIndex);
	void StereographicProjection(TArray<FVector>& Points);

	void CheckUnusedVertices();

	void GeoDelauny();
	void GeoDelaunayFrom();
};