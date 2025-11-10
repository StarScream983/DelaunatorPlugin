// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Delaunator.h"
#include "MathShim.h"
#include <functional>
#include <array>
#include "GeoDelaunatorComponent.generated.h"


#define _PI UE_DOUBLE_PI
#define _TAU UE_DOUBLE_TWO_PI
#define DEGREES (180. / UE_DOUBLE_PI)
#define RADIANS (UE_DOUBLE_PI / 180.)

struct FCompose
{
public:
	static std::function<FVector2D(FVector2D)> Compose(
		std::function<FVector2D(FVector2D)> a,
		std::function<FVector2D(FVector2D)> b) {

		return[a, b](FVector2D coordinates) {
			FVector2D result = b(coordinates);
			return a(result);
			};
	}
};

struct FGeoRotation
{
//public:

	FGeoRotation() {};
	// Pivot in degrees
	FGeoRotation(FVector2D pivot){
		Rotate = RotateRadians(pivot.X, pivot.Y);
	}

	std::function<FVector2D(FVector2D)> Rotate;

	// the main function, will not be called
	FVector2D Forward(FVector2D coordinates)
	{
		// call Rotate with "coordinates"
		return FVector2D();
	}

	// the invert of the main function, this one will be called
	FVector2D Invert(FVector2D coordinates)
	{
		//UE_LOG(LogTemp, Warning, TEXT("GEO INVERT !!"));
		coordinates = Rotate(coordinates);
		return coordinates * DEGREES;
	}

	static std::function<FVector2D(FVector2D)> RotateRadians(double deltaLambda, double deltaPhi, double deltaGamma = 0.)
	{
		deltaLambda = FMath::Fmod(deltaLambda, _TAU);
		if (deltaLambda != 0.)
		{
			if(deltaPhi != 0. || deltaGamma != 0.)
			{
				return FCompose::Compose(RotationLambda(-deltaLambda), RotationPhiGamma(deltaPhi, deltaGamma));
			}
			else
			{
				return RotationLambda(-deltaLambda);
			}
		}
		else
		{
			if (deltaPhi != 0. || deltaGamma != 0.)
			{
				return RotationLambda(-deltaLambda);
			}
			else
			{
				return RotationIdentity();
			}
		}
	}

	static std::function<FVector2D(FVector2D)> RotationIdentity()
	{
		return [](FVector2D lambdaPhi) {
				double lambda = lambdaPhi.X;
				if (FMath::Abs(lambda) > _PI) lambda -= std::round(lambda / _TAU) * _TAU;
				return FVector2D(lambda, lambdaPhi.Y);
			};
	}

	static std::function<FVector2D(FVector2D)> RotationLambda(double deltaLambda)
	{
		return [deltaLambda](FVector2D coordinates) {
			double lambda = coordinates.X;
			lambda += deltaLambda;
			if (FMath::Abs(lambda) > _PI) lambda -= std::round(lambda / _TAU) * _TAU;
				return FVector2D(lambda, coordinates.Y);
			};
	}

	static std::function<FVector2D(FVector2D)> RotationPhiGamma(double deltaPhi, double deltaGamma = 0.)
	{
		return [deltaPhi, deltaGamma](FVector2D coordinates) {
			double cosDeltaPhi = MathShim::SCos(deltaPhi),
				sinDeltaPhi = MathShim::SSin(deltaPhi),
				cosDeltaGamma = MathShim::SCos(deltaGamma),
				sinDeltaGamma = MathShim::SSin(deltaGamma);

			double lambda = coordinates.X, phi = coordinates.Y;

			double cosPhi = MathShim::SCos(phi),
				x = MathShim::SCos(lambda) * cosPhi,
				y = MathShim::SSin(lambda) * cosPhi,
				z = MathShim::SSin(phi),
				k = z * cosDeltaGamma - y * sinDeltaGamma;
			return FVector2D(MathShim::SAtan2(y * cosDeltaGamma + z * sinDeltaGamma, x * cosDeltaPhi + k * sinDeltaPhi),
				MathShim::SAsin(k * cosDeltaPhi - x * sinDeltaPhi));
		};
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
			double cy = MathShim::SCos(point.Y), k = 1 + MathShim::SCos(point.X) * cy;
			return FVector2D(cy * MathShim::SSin(point.X) / k, MathShim::SSin(point.Y) / k);
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
		double cosAlpha = MathShim::SCos(alpha),
			sinAlpha = MathShim::SSin(alpha),
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

		std::function<FVector2D(FVector2D)> transform = scaleTranslateRotate(_k, _x - center[0], _y - center[1], _sx, _sy, _alpha);
		_rotate = FGeoRotation::RotateRadians(_deltaLambda, _deltaPhi, _deltaGamma);
		_projectTransform = FCompose::Compose(_project, transform);
		_projectRotateTransform = FCompose::Compose(_rotate, _projectTransform);
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

	UPROPERTY()
	UDelaunator* Delaunator = nullptr;

	TArray<FVector> FibonacciPoints;
	std::vector<double> coords;

public:

	void GeoDelauny(std::vector<FVector2D> inPoints);
	UDelaunator* GeoDelaunayFrom(std::vector<FVector2D> inPoints);
};
