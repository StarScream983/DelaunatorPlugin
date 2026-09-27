// PlanetNoise.h — header-only 3D noise (Perlin, Simplex, Value, FBm, domain warp)
#pragma once

#include "CoreMinimal.h"
#include "Noise/PlanetNoiseOpenSimplex.h"

namespace PlanetNoise
{
	/** Quintic fade: 6t^5 - 15t^4 + 10t^3 */
	FORCEINLINE float Fade(float T)
	{
		return T * T * T * (T * (T * 6.f - 15.f) + 10.f);
	}

	FORCEINLINE float GradDot(int32 Hash, float X, float Y, float Z)
	{
		// 12 cube-edge directions (Perlin)
		switch (Hash & 15)
		{
		case  0: return  X + Y;
		case  1: return -X + Y;
		case  2: return  X - Y;
		case  3: return -X - Y;
		case  4: return  X + Z;
		case  5: return -X + Z;
		case  6: return  X - Z;
		case  7: return -X - Z;
		case  8: return  Y + Z;
		case  9: return -Y + Z;
		case 10: return  Y - Z;
		case 11: return -Y - Z;
		case 12: return  X + Y;
		case 13: return -X + Y;
		case 14: return -Y + Z;
		default: return  Y - Z;
		}
	}

	FORCEINLINE int32 Hash3(int32 X, int32 Y, int32 Z, int32 Seed)
	{
		int32 H = Seed;
		H ^= X * 374761393;
		H ^= Y * 668265263;
		H ^= Z * 2147483647;
		H = (H ^ (H >> 13)) * 1274126177;
		return H ^ (H >> 16);
	}

	/** Classic Perlin (gradient noise on a cubic lattice). ≈ [-1, 1] */
	inline float Perlin3D(float X, float Y, float Z, int32 Seed = 1337)
	{
		const int32 Xi = FMath::FloorToInt(X);
		const int32 Yi = FMath::FloorToInt(Y);
		const int32 Zi = FMath::FloorToInt(Z);
		const float Xf = X - Xi;
		const float Yf = Y - Yi;
		const float Zf = Z - Zi;
		const float U = Fade(Xf);
		const float V = Fade(Yf);
		const float W = Fade(Zf);

		const float N000 = GradDot(Hash3(Xi,     Yi,     Zi,     Seed), Xf,       Yf,       Zf);
		const float N001 = GradDot(Hash3(Xi,     Yi,     Zi + 1, Seed), Xf,       Yf,       Zf - 1.f);
		const float N010 = GradDot(Hash3(Xi,     Yi + 1, Zi,     Seed), Xf,       Yf - 1.f, Zf);
		const float N011 = GradDot(Hash3(Xi,     Yi + 1, Zi + 1, Seed), Xf,       Yf - 1.f, Zf - 1.f);
		const float N100 = GradDot(Hash3(Xi + 1, Yi,     Zi,     Seed), Xf - 1.f, Yf,       Zf);
		const float N101 = GradDot(Hash3(Xi + 1, Yi,     Zi + 1, Seed), Xf - 1.f, Yf,       Zf - 1.f);
		const float N110 = GradDot(Hash3(Xi + 1, Yi + 1, Zi,     Seed), Xf - 1.f, Yf - 1.f, Zf);
		const float N111 = GradDot(Hash3(Xi + 1, Yi + 1, Zi + 1, Seed), Xf - 1.f, Yf - 1.f, Zf - 1.f);

		const float X00 = FMath::Lerp(N000, N100, U);
		const float X01 = FMath::Lerp(N001, N101, U);
		const float X10 = FMath::Lerp(N010, N110, U);
		const float X11 = FMath::Lerp(N011, N111, U);
		const float Y0  = FMath::Lerp(X00, X10, V);
		const float Y1  = FMath::Lerp(X01, X11, V);
		return FMath::Lerp(Y0, Y1, W);
	}

	/** 3D simplex (skewed lattice). ≈ [-1, 1] */
	inline float Simplex3D(float X, float Y, float Z, int32 Seed = 1337)
	{
		constexpr float F3 = 1.f / 3.f;
		constexpr float G3 = 1.f / 6.f;

		const float S = (X + Y + Z) * F3;
		const int32 I = FMath::FloorToInt(X + S);
		const int32 J = FMath::FloorToInt(Y + S);
		const int32 K = FMath::FloorToInt(Z + S);

		const float T = (I + J + K) * G3;
		const float X0 = X - (I - T);
		const float Y0 = Y - (J - T);
		const float Z0 = Z - (K - T);

		int32 I1, J1, K1, I2, J2, K2;
		if (X0 >= Y0)
		{
			if (Y0 >= Z0)      { I1 = 1; J1 = 0; K1 = 0; I2 = 1; J2 = 1; K2 = 0; }
			else if (X0 >= Z0) { I1 = 1; J1 = 0; K1 = 0; I2 = 1; J2 = 0; K2 = 1; }
			else               { I1 = 0; J1 = 0; K1 = 1; I2 = 1; J2 = 0; K2 = 1; }
		}
		else
		{
			if (Y0 < Z0)       { I1 = 0; J1 = 0; K1 = 1; I2 = 0; J2 = 1; K2 = 1; }
			else if (X0 < Z0)  { I1 = 0; J1 = 1; K1 = 0; I2 = 0; J2 = 1; K2 = 1; }
			else               { I1 = 0; J1 = 1; K1 = 0; I2 = 1; J2 = 1; K2 = 0; }
		}

		const float X1 = X0 - I1 + G3;
		const float Y1 = Y0 - J1 + G3;
		const float Z1 = Z0 - K1 + G3;
		const float X2 = X0 - I2 + 2.f * G3;
		const float Y2 = Y0 - J2 + 2.f * G3;
		const float Z2 = Z0 - K2 + 2.f * G3;
		const float X3 = X0 - 1.f + 3.f * G3;
		const float Y3 = Y0 - 1.f + 3.f * G3;
		const float Z3 = Z0 - 1.f + 3.f * G3;

		auto Corner = [&](int32 II, int32 JJ, int32 KK, float Xx, float Yy, float Zz) -> float
		{
			float T0 = 0.6f - Xx * Xx - Yy * Yy - Zz * Zz;
			if (T0 < 0.f)
			{
				return 0.f;
			}
			T0 *= T0;
			return T0 * T0 * GradDot(Hash3(II, JJ, KK, Seed), Xx, Yy, Zz);
		};

		float N = 0.f;
		N += Corner(I,      J,      K,      X0, Y0, Z0);
		N += Corner(I + I1, J + J1, K + K1, X1, Y1, Z1);
		N += Corner(I + I2, J + J2, K + K2, X2, Y2, Z2);
		N += Corner(I + 1,  J + 1,  K + 1,  X3, Y3, Z3);
		return 32.f * N; // scale toward ~[-1,1]
	}

	/** Hash → float in ≈ [-1, 1] for value noise lattice corners. */
	FORCEINLINE float HashToFloat(int32 Hash)
	{
		return (static_cast<float>(Hash & 0x7fffffff) / 1073741824.f) - 1.f;
	}

	/** Value noise on a cubic lattice (scalar at corners, quintic blend). ≈ [-1, 1] */
	inline float Value3D(float X, float Y, float Z, int32 Seed = 1337)
	{
		const int32 Xi = FMath::FloorToInt(X);
		const int32 Yi = FMath::FloorToInt(Y);
		const int32 Zi = FMath::FloorToInt(Z);
		const float Xf = X - Xi;
		const float Yf = Y - Yi;
		const float Zf = Z - Zi;
		const float U = Fade(Xf);
		const float V = Fade(Yf);
		const float W = Fade(Zf);

		const float N000 = HashToFloat(Hash3(Xi,     Yi,     Zi,     Seed));
		const float N001 = HashToFloat(Hash3(Xi,     Yi,     Zi + 1, Seed));
		const float N010 = HashToFloat(Hash3(Xi,     Yi + 1, Zi,     Seed));
		const float N011 = HashToFloat(Hash3(Xi,     Yi + 1, Zi + 1, Seed));
		const float N100 = HashToFloat(Hash3(Xi + 1, Yi,     Zi,     Seed));
		const float N101 = HashToFloat(Hash3(Xi + 1, Yi,     Zi + 1, Seed));
		const float N110 = HashToFloat(Hash3(Xi + 1, Yi + 1, Zi,     Seed));
		const float N111 = HashToFloat(Hash3(Xi + 1, Yi + 1, Zi + 1, Seed));

		const float X00 = FMath::Lerp(N000, N100, U);
		const float X01 = FMath::Lerp(N001, N101, U);
		const float X10 = FMath::Lerp(N010, N110, U);
		const float X11 = FMath::Lerp(N011, N111, U);
		const float Y0  = FMath::Lerp(X00, X10, V);
		const float Y1  = FMath::Lerp(X01, X11, V);
		return FMath::Lerp(Y0, Y1, W);
	}

	/** Alias: Perlin/Simplex are both gradient noise. */
	FORCEINLINE float Gradient3D(float X, float Y, float Z, int32 Seed = 1337)
	{
		return Perlin3D(X, Y, Z, Seed);
	}

	enum class EBase : uint8 { Perlin, Simplex, Value, OpenSimplex, OpenSimplex2F, OpenSimplex2S };

	FORCEINLINE float Sample(EBase Base, float X, float Y, float Z, int32 Seed)
	{
		switch (Base)
		{
		case EBase::Value:        return Value3D(X, Y, Z, Seed);
		case EBase::Perlin:       return Perlin3D(X, Y, Z, Seed);
		case EBase::OpenSimplex:  return OpenSimplex3D(X, Y, Z, Seed);
		case EBase::OpenSimplex2F: return OpenSimplex2F_3D(X, Y, Z, Seed);
		case EBase::OpenSimplex2S: return OpenSimplex2S_3D(X, Y, Z, Seed);
		case EBase::Simplex:
		default:                  return Simplex3D(X, Y, Z, Seed);
		}
	}

	/** Fractional Brownian motion. */
	inline float FBm(
		float X, float Y, float Z,
		int32 Octaves = 5,
		float Lacunarity = 2.f,
		float Gain = 0.5f,
		EBase Base = EBase::Simplex,
		int32 Seed = 1337)
	{
		float Sum = 0.f;
		float Amp = 1.f;
		float Freq = 1.f;
		float Norm = 0.f;
		for (int32 O = 0; O < Octaves; ++O)
		{
			Sum += Amp * Sample(Base, X * Freq, Y * Freq, Z * Freq, Seed + O * 1013);
			Norm += Amp;
			Freq *= Lacunarity;
			Amp *= Gain;
		}
		return Norm > KINDA_SMALL_NUMBER ? (Sum / Norm) : 0.f;
	}

	/**
	 * Progressive fractal domain warp (FastNoise2-style).
	 * Octaves == 1 matches a single WarpPoint displace.
	 * Each later octave warps from the already-warped position (finer swirl).
	 */
	inline void WarpPointFractal(
		float& X, float& Y, float& Z,
		float Amplitude = 1.f,
		float WarpFreq = 1.f,
		int32 Octaves = 1,
		float Lacunarity = 2.f,
		float Gain = 0.5f,
		EBase Base = EBase::Simplex,
		int32 Seed = 1337)
	{
		float Amp = Amplitude;
		float Freq = WarpFreq;
		const int32 N = FMath::Max(1, Octaves);
		for (int32 O = 0; O < N; ++O)
		{
			const float Wx = Sample(Base, X * Freq,        Y * Freq,        Z * Freq,        Seed + O * 1013);
			const float Wy = Sample(Base, X * Freq + 5.2f, Y * Freq + 1.3f, Z * Freq + 2.8f, Seed + O * 1013 + 17);
			const float Wz = Sample(Base, X * Freq + 9.3f, Y * Freq + 7.8f, Z * Freq + 4.1f, Seed + O * 1013 + 31);
			X += Amp * Wx;
			Y += Amp * Wy;
			Z += Amp * Wz;
			Freq *= Lacunarity;
			Amp *= Gain;
		}
	}

	/**
	 * IQ domain warp f(p + h(p)) in 3D (Orbis WarpPosition3D / iquilezles.org/articles/warp).
	 * h(p) = (FBm(p), FBm(p+o1), FBm(p+o2)); WarpOctaves = FBm octaves for h only.
	 */
	inline void WarpPositionIq3D(
		float& X, float& Y, float& Z,
		float WarpStrength = 0.5f,
		int32 WarpOctaves = 6,
		float Lacunarity = 2.f,
		float Gain = 0.5f,
		EBase Base = EBase::Simplex,
		int32 Seed = 1337)
	{
		const float Ox = FBm(X,        Y,        Z,        WarpOctaves, Lacunarity, Gain, Base, Seed);
		const float Oy = FBm(X + 5.2f, Y + 1.3f, Z + 2.8f, WarpOctaves, Lacunarity, Gain, Base, Seed + 911);
		const float Oz = FBm(X + 1.7f, Y + 9.2f, Z + 4.1f, WarpOctaves, Lacunarity, Gain, Base, Seed + 1777);
		X += WarpStrength * Ox;
		Y += WarpStrength * Oy;
		Z += WarpStrength * Oz;
	}

	/**
	 * Domain warp: offset (X,Y,Z) by noise, then sample.
	 * Amplitude is in the same units as the input coords.
	 */
	inline float Warp(
		float X, float Y, float Z,
		float Amplitude = 1.f,
		float WarpFreq = 1.f,
		EBase Base = EBase::Simplex,
		int32 Seed = 1337)
	{
		WarpPointFractal(X, Y, Z, Amplitude, WarpFreq, /*Octaves*/ 1, 2.f, 0.5f, Base, Seed);
		return Sample(Base, X, Y, Z, Seed + 53);
	}

	/** Single-octave warp (coords only). */
	inline void WarpPoint(
		float& X, float& Y, float& Z,
		float Amplitude = 1.f,
		float WarpFreq = 1.f,
		EBase Base = EBase::Simplex,
		int32 Seed = 1337)
	{
		WarpPointFractal(X, Y, Z, Amplitude, WarpFreq, 1, 2.f, 0.5f, Base, Seed);
	}

	/**
	 * Quilez-style fBm: amp starts at 0.5, geometric series (~[-1,1]), no extra normalize.
	 * 2D rotation between octaves (his Shadertoy fbm).
	 */
	inline float FBmIq(
		float X, float Y,
		int32 Octaves = 5,
		int32 Seed = 1337,
		EBase Base = EBase::Simplex,
		bool bRotate = true)
	{
		constexpr float M00 = 0.80f, M01 = 0.60f, M10 = -0.60f, M11 = 0.80f;

		float Sum = 0.f;
		float Amp = 0.5f;
		float Px = X;
		float Py = Y;
		const int32 N = FMath::Max(1, Octaves);
		for (int32 O = 0; O < N; ++O)
		{
			Sum += Amp * Sample(Base, Px, Py, 0.f, Seed + O * 1013);
			Amp *= 0.5f;
			if (bRotate)
			{
				const float Nx = M00 * Px + M01 * Py;
				const float Ny = M10 * Px + M11 * Py;
				Px = Nx * 2.02f;
				Py = Ny * 2.03f;
			}
			else
			{
				Px *= 2.f;
				Py *= 2.f;
			}
		}
		return Sum;
	}

	/**
	 * Quilez one-warp (2D): fbm(p + Amp * q), q = (fbm(p), fbm(p+offset)).
	 * https://iquilezles.org/articles/warp/ — use Amplitude ~= 4.
	 */
	inline float PatternQuilez(
		float X, float Y, float Z,
		float Amplitude = 4.f,
		int32 Octaves = 5,
		float Lacunarity = 2.f,
		float Gain = 0.5f,
		EBase Base = EBase::Simplex,
		int32 Seed = 1337)
	{
		(void)Z;
		(void)Lacunarity;
		(void)Gain;

		const float Qx = FBmIq(X,        Y,        Octaves, Seed,      Base);
		const float Qy = FBmIq(X + 5.2f, Y + 1.3f, Octaves, Seed + 17, Base);
		return FBmIq(X + Amplitude * Qx, Y + Amplitude * Qy, Octaves, Seed + 53, Base);
	}

	/**
	 * Quilez nested (2D): fbm(p + Amp * r), r from fbm(p + Amp * q + offsets).
	 */
	inline float PatternQuilezNested(
		float X, float Y, float Z,
		float Amplitude = 4.f,
		int32 Octaves = 5,
		float Lacunarity = 2.f,
		float Gain = 0.5f,
		EBase Base = EBase::Simplex,
		int32 Seed = 1337)
	{
		(void)Z;
		(void)Lacunarity;
		(void)Gain;

		const float Qx = FBmIq(X,        Y,        Octaves, Seed,      Base);
		const float Qy = FBmIq(X + 5.2f, Y + 1.3f, Octaves, Seed + 17, Base);

		const float Rx = FBmIq(X + Amplitude * Qx + 1.7f, Y + Amplitude * Qy + 9.2f, Octaves, Seed + 71, Base);
		const float Ry = FBmIq(X + Amplitude * Qx + 8.3f, Y + Amplitude * Qy + 2.8f, Octaves, Seed + 97, Base);

		return FBmIq(X + Amplitude * Rx, Y + Amplitude * Ry, Octaves, Seed + 131, Base);
	}
}
