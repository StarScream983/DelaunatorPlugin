// PlanetNoiseOpenSimplex.h — OpenSimplex 2014 + OpenSimplex2F/S (3D)
// OpenSimplex 2014: Kurt Spencer / Stephen M. Cameron port
// OpenSimplex2F/S: Auburn FastNoiseLite (MIT) / K.jpg OpenSimplex2
#pragma once

#include "CoreMinimal.h"

namespace PlanetNoise
{
namespace OpenSimplexDetail
{
	static constexpr int32 PRIME_X = 501125321;
	static constexpr int32 PRIME_Y = 1136930381;
	static constexpr int32 PRIME_Z = 1720413743;

	static constexpr float OS14_STRETCH_3D = -1.f / 6.f;
	static constexpr float OS14_SQUISH_3D = 1.f / 3.f;
	static constexpr float OS14_NORM_3D = 103.f;

	FORCEINLINE int32 Os2FastFloor(float F) { return (F >= 0.f) ? static_cast<int32>(F) : static_cast<int32>(F) - 1; }
	FORCEINLINE int32 Os2FastRound(float F) { return (F >= 0.f) ? static_cast<int32>(F + 0.5f) : static_cast<int32>(F - 0.5f); }
	FORCEINLINE int32 Os14FastFloor(float F)
	{
		const int32 Xi = static_cast<int32>(F);
		return (F < static_cast<float>(Xi)) ? (Xi - 1) : Xi;
	}

	FORCEINLINE int32 Os2Hash3D(int32 Seed, int32 XPrimed, int32 YPrimed, int32 ZPrimed)
	{
		int32 Hash = Seed ^ XPrimed ^ YPrimed ^ ZPrimed;
		Hash *= 0x27d4eb2d;
		return Hash;
	}

	static constexpr float GRADIENTS_3D[] = 
	{
	    0, 1, 1, 0,  0,-1, 1, 0,  0, 1,-1, 0,  0,-1,-1, 0,
	    1, 0, 1, 0, -1, 0, 1, 0,  1, 0,-1, 0, -1, 0,-1, 0,
	    1, 1, 0, 0, -1, 1, 0, 0,  1,-1, 0, 0, -1,-1, 0, 0,
	    0, 1, 1, 0,  0,-1, 1, 0,  0, 1,-1, 0,  0,-1,-1, 0,
	    1, 0, 1, 0, -1, 0, 1, 0,  1, 0,-1, 0, -1, 0,-1, 0,
	    1, 1, 0, 0, -1, 1, 0, 0,  1,-1, 0, 0, -1,-1, 0, 0,
	    0, 1, 1, 0,  0,-1, 1, 0,  0, 1,-1, 0,  0,-1,-1, 0,
	    1, 0, 1, 0, -1, 0, 1, 0,  1, 0,-1, 0, -1, 0,-1, 0,
	    1, 1, 0, 0, -1, 1, 0, 0,  1,-1, 0, 0, -1,-1, 0, 0,
	    0, 1, 1, 0,  0,-1, 1, 0,  0, 1,-1, 0,  0,-1,-1, 0,
	    1, 0, 1, 0, -1, 0, 1, 0,  1, 0,-1, 0, -1, 0,-1, 0,
	    1, 1, 0, 0, -1, 1, 0, 0,  1,-1, 0, 0, -1,-1, 0, 0,
	    0, 1, 1, 0,  0,-1, 1, 0,  0, 1,-1, 0,  0,-1,-1, 0,
	    1, 0, 1, 0, -1, 0, 1, 0,  1, 0,-1, 0, -1, 0,-1, 0,
	    1, 1, 0, 0, -1, 1, 0, 0,  1,-1, 0, 0, -1,-1, 0, 0,
	    1, 1, 0, 0,  0,-1, 1, 0, -1, 1, 0, 0,  0,-1,-1, 0
	};

	FORCEINLINE float Os2GradCoord3D(int32 Seed, int32 XPrimed, int32 YPrimed, int32 ZPrimed, float Xd, float Yd, float Zd)
	{
		int32 Hash = Os2Hash3D(Seed, XPrimed, YPrimed, ZPrimed);
		Hash ^= Hash >> 15;
		Hash &= 63 << 2;
		return Xd * GRADIENTS_3D[Hash] + Yd * GRADIENTS_3D[Hash | 1] + Zd * GRADIENTS_3D[Hash | 2];
	}

	static constexpr int8 GRADIENTS3D_OS14[] = {
		-11,  4,  4,     -4,  11,  4,    -4,  4,  11,
		 11,  4,  4,      4,  11,  4,     4,  4,  11,
		-11, -4,  4,     -4, -11,  4,    -4, -4,  11,
		 11, -4,  4,      4, -11,  4,     4, -4,  11,
		-11,  4, -4,     -4,  11, -4,    -4,  4, -11,
		 11,  4, -4,      4,  11, -4,     4,  4, -11,
		-11, -4, -4,     -4, -11, -4,    -4, -4, -11,
		 11, -4, -4,      4, -11, -4,     4, -4, -11,
	};

	FORCEINLINE float Extrapolate3(const int16* Perm, const int16* PermGradIndex3D, int32 Xsb, int32 Ysb, int32 Zsb, float Dx, float Dy, float Dz)
	{
		const int32 Index = PermGradIndex3D[(Perm[(Perm[Xsb & 0xFF] + Ysb) & 0xFF] + Zsb) & 0xFF];
		return GRADIENTS3D_OS14[Index] * Dx + GRADIENTS3D_OS14[Index + 1] * Dy + GRADIENTS3D_OS14[Index + 2] * Dz;
	}

	inline void BuildOpenSimplex2014Perm(int32 Seed, int16* OutPerm, int16* OutPermGradIndex3D)
	{
		int16 Source[256];
		for (int32 i = 0; i < 256; ++i)
		{
			Source[i] = static_cast<int16>(i);
		}
		uint64 SeedU = static_cast<uint64>(static_cast<int64>(Seed));
		SeedU = SeedU * 6364136223846793005ULL + 1442695040888963407ULL;
		SeedU = SeedU * 6364136223846793005ULL + 1442695040888963407ULL;
		SeedU = SeedU * 6364136223846793005ULL + 1442695040888963407ULL;
		for (int32 i = 255; i >= 0; --i)
		{
			SeedU = SeedU * 6364136223846793005ULL + 1442695040888963407ULL;
			int32 r = static_cast<int32>((SeedU + 31) % static_cast<uint64>(i + 1));
			if (r < 0)
			{
				r += (i + 1);
			}
			OutPerm[i] = Source[r];
			OutPermGradIndex3D[i] = static_cast<int16>((OutPerm[i] % (UE_ARRAY_COUNT(GRADIENTS3D_OS14) / 3)) * 3);
			Source[r] = Source[i];
		}
	}

	FORCEINLINE void EnsureOpenSimplex2014Perm(int32 Seed, const int16*& OutPerm, const int16*& OutPermGrad)
	{
		thread_local int32 CachedSeed = MIN_int32;
		thread_local int16 Perm[256];
		thread_local int16 PermGradIndex3D[256];
		if (CachedSeed != Seed)
		{
			BuildOpenSimplex2014Perm(Seed, Perm, PermGradIndex3D);
			CachedSeed = Seed;
		}
		OutPerm = Perm;
		OutPermGrad = PermGradIndex3D;
	}

	static inline float SingleOpenSimplex2F3D(int32 Seed, float X, float Y, float Z)
	{
	    // 3D OpenSimplex2 case uses two offset rotated cube grids.
	
	    /*
	     * --- Rotation moved to TransformNoiseCoordinate method ---
	     * const float R3 = (float)(2.0 / 3.0);
	     * float r = (x + y + z) * R3; // Rotation, not skew
	     * x = r - x; y = r - y; z = r - z;
	     */
	
	    int32 i = Os2FastRound(X);
	    int32 j = Os2FastRound(Y);
	    int32 k = Os2FastRound(Z);
	    float X0 = (float)(X - i);
	    float Y0 = (float)(Y - j);
	    float Z0 = (float)(Z - k);
	
	    int32 xNSign = (int)(-1.0f - X0) | 1;
	    int32 yNSign = (int)(-1.0f - Y0) | 1;
	    int32 zNSign = (int)(-1.0f - Z0) | 1;
	
	    float ax0 = xNSign * -X0;
	    float ay0 = yNSign * -Y0;
	    float az0 = zNSign * -Z0;
	
	    i *= PRIME_X;
	    j *= PRIME_Y;
	    k *= PRIME_Z;
	
	    float value = 0;
	    float a = (0.6f - X0 * X0) - (Y0 * Y0 + Z0 * Z0);
	
	    for (int l = 0; ; l++)
	    {
	        if (a > 0)
	        {
	            value += (a * a) * (a * a) * Os2GradCoord3D(Seed, i, j, k, X0, Y0, Z0);
	        }
	
	        float b = a + 1;
	        int32 i1 = i;
	        int32 j1 = j;
	        int32 k1 = k;
	        float X1 = X0;
	        float Y1 = Y0;
	        float Z1 = Z0;
	        if (ax0 >= ay0 && ax0 >= az0)
	        {
	            X1 += xNSign;
	            b -= xNSign * 2 * X1;
	            i1 -= xNSign * PRIME_X;
	        }
	        else if (ay0 > ax0 && ay0 >= az0)
	        {
	            Y1 += yNSign;
	            b -= yNSign * 2 * Y1;
	            j1 -= yNSign * PRIME_Y;
	        }
	        else
	        {
	            Z1 += zNSign;
	            b -= zNSign * 2 * Z1;
	            k1 -= zNSign * PRIME_Z;
	        }
	
	        if (b > 0)
	        {
	            value += (b * b) * (b * b) * Os2GradCoord3D(Seed, i1, j1, k1, X1, Y1, Z1);
	        }
	
	        if (l == 1)
	            break;
	
	        ax0 = 0.5f - ax0;
	        ay0 = 0.5f - ay0;
	        az0 = 0.5f - az0;
	
	        X0 = xNSign * ax0;
	        Y0 = yNSign * ay0;
	        Z0 = zNSign * az0;
	
	        a += (0.75f - ax0) - (ay0 + az0);
	
	        i += (xNSign >> 1) & PRIME_X;
	        j += (yNSign >> 1) & PRIME_Y;
	        k += (zNSign >> 1) & PRIME_Z;
	
	        xNSign = -xNSign;
	        yNSign = -yNSign;
	        zNSign = -zNSign;
	
	        Seed = ~Seed;
	    }
	
	    return value * 32.69428253173828125f;
	}
	
	

	static inline float SingleOpenSimplex2S3D(int32 Seed, float X, float Y, float Z)
	{
	    // 3D OpenSimplex2S case uses two offset rotated cube grids.
	
	    /*
	     * --- Rotation moved to TransformNoiseCoordinate method ---
	     * const float R3 = (float)(2.0 / 3.0);
	     * float r = (x + y + z) * R3; // Rotation, not skew
	     * x = r - x; y = r - y; z = r - z;
	     */
	
	    int32 i = Os2FastFloor(X);
	    int32 j = Os2FastFloor(Y);
	    int32 k = Os2FastFloor(Z);
	    float Xi = (float)(X - i);
	    float Yi = (float)(Y - j);
	    float Zi = (float)(Z - k);
	
	    i *= PRIME_X;
	    j *= PRIME_Y;
	    k *= PRIME_Z;
	    int32 Seed2 = Seed + 1293373;
	
	    int32 xNMask = (int)(-0.5f - Xi);
	    int32 yNMask = (int)(-0.5f - Yi);
	    int32 zNMask = (int)(-0.5f - Zi);
	
	    float X0 = Xi + xNMask;
	    float Y0 = Yi + yNMask;
	    float Z0 = Zi + zNMask;
	    float a0 = 0.75f - X0 * X0 - Y0 * Y0 - Z0 * Z0;
	    float value = (a0 * a0) * (a0 * a0) * Os2GradCoord3D(Seed,
	                                                          i + (xNMask & PRIME_X), j + (yNMask & PRIME_Y), k + (zNMask & PRIME_Z), X0, Y0, Z0);
	
	    float X1 = Xi - 0.5f;
	    float Y1 = Yi - 0.5f;
	    float Z1 = Zi - 0.5f;
	    float a1 = 0.75f - X1 * X1 - Y1 * Y1 - Z1 * Z1;
	    value += (a1 * a1) * (a1 * a1) * Os2GradCoord3D(Seed2,
	                                                     i + PRIME_X, j + PRIME_Y, k + PRIME_Z, X1, Y1, Z1);
	
	    float xAFlipMask0 = ((xNMask | 1) << 1) * X1;
	    float yAFlipMask0 = ((yNMask | 1) << 1) * Y1;
	    float zAFlipMask0 = ((zNMask | 1) << 1) * Z1;
	    float xAFlipMask1 = (-2 - (xNMask << 2)) * X1 - 1.0f;
	    float yAFlipMask1 = (-2 - (yNMask << 2)) * Y1 - 1.0f;
	    float zAFlipMask1 = (-2 - (zNMask << 2)) * Z1 - 1.0f;
	
	    bool bSkip5 = false;
	    float a2 = xAFlipMask0 + a0;
	    if (a2 > 0)
	    {
	        float x2 = X0 - (xNMask | 1);
	        float y2 = Y0;
	        float z2 = Z0;
	        value += (a2 * a2) * (a2 * a2) * Os2GradCoord3D(Seed,
	                                                         i + (~xNMask & PRIME_X), j + (yNMask & PRIME_Y), k + (zNMask & PRIME_Z), x2, y2, z2);
	    }
	    else
	    {
	        float a3 = yAFlipMask0 + zAFlipMask0 + a0;
	        if (a3 > 0)
	        {
	            float x3 = X0;
	            float y3 = Y0 - (yNMask | 1);
	            float z3 = Z0 - (zNMask | 1);
	            value += (a3 * a3) * (a3 * a3) * Os2GradCoord3D(Seed,
	                                                             i + (xNMask & PRIME_X), j + (~yNMask & PRIME_Y), k + (~zNMask & PRIME_Z), x3, y3, z3);
	        }
	
	        float a4 = xAFlipMask1 + a1;
	        if (a4 > 0)
	        {
	            float x4 = (xNMask | 1) + X1;
	            float y4 = Y1;
	            float z4 = Z1;
	            value += (a4 * a4) * (a4 * a4) * Os2GradCoord3D(Seed2,
	                                                             i + (xNMask & (PRIME_X * 2)), j + PRIME_Y, k + PRIME_Z, x4, y4, z4);
	            bSkip5 = true;
	        }
	    }
	
	    bool bSkip9 = false;
	    float a6 = yAFlipMask0 + a0;
	    if (a6 > 0)
	    {
	        float x6 = X0;
	        float y6 = Y0 - (yNMask | 1);
	        float z6 = Z0;
	        value += (a6 * a6) * (a6 * a6) * Os2GradCoord3D(Seed,
	                                                         i + (xNMask & PRIME_X), j + (~yNMask & PRIME_Y), k + (zNMask & PRIME_Z), x6, y6, z6);
	    }
	    else
	    {
	        float a7 = xAFlipMask0 + zAFlipMask0 + a0;
	        if (a7 > 0)
	        {
	            float x7 = X0 - (xNMask | 1);
	            float y7 = Y0;
	            float z7 = Z0 - (zNMask | 1);
	            value += (a7 * a7) * (a7 * a7) * Os2GradCoord3D(Seed,
	                                                             i + (~xNMask & PRIME_X), j + (yNMask & PRIME_Y), k + (~zNMask & PRIME_Z), x7, y7, z7);
	        }
	
	        float a8 = yAFlipMask1 + a1;
	        if (a8 > 0)
	        {
	            float x8 = X1;
	            float y8 = (yNMask | 1) + Y1;
	            float z8 = Z1;
	            value += (a8 * a8) * (a8 * a8) * Os2GradCoord3D(Seed2,
	                                                             i + PRIME_X, j + (yNMask & (PRIME_Y << 1)), k + PRIME_Z, x8, y8, z8);
	            bSkip9 = true;
	        }
	    }
	
	    bool bSkipD = false;
	    float aA = zAFlipMask0 + a0;
	    if (aA > 0)
	    {
	        float xA = X0;
	        float yA = Y0;
	        float zA = Z0 - (zNMask | 1);
	        value += (aA * aA) * (aA * aA) * Os2GradCoord3D(Seed,
	                                                         i + (xNMask & PRIME_X), j + (yNMask & PRIME_Y), k + (~zNMask & PRIME_Z), xA, yA, zA);
	    }
	    else
	    {
	        float aB = xAFlipMask0 + yAFlipMask0 + a0;
	        if (aB > 0)
	        {
	            float xB = X0 - (xNMask | 1);
	            float yB = Y0 - (yNMask | 1);
	            float zB = Z0;
	            value += (aB * aB) * (aB * aB) * Os2GradCoord3D(Seed,
	                                                             i + (~xNMask & PRIME_X), j + (~yNMask & PRIME_Y), k + (zNMask & PRIME_Z), xB, yB, zB);
	        }
	
	        float aC = zAFlipMask1 + a1;
	        if (aC > 0)
	        {
	            float xC = X1;
	            float yC = Y1;
	            float zC = (zNMask | 1) + Z1;
	            value += (aC * aC) * (aC * aC) * Os2GradCoord3D(Seed2,
	                                                             i + PRIME_X, j + PRIME_Y, k + (zNMask & (PRIME_Z << 1)), xC, yC, zC);
	            bSkipD = true;
	        }
	    }
	
	    if (!bSkip5)
	    {
	        float a5 = yAFlipMask1 + zAFlipMask1 + a1;
	        if (a5 > 0)
	        {
	            float x5 = X1;
	            float y5 = (yNMask | 1) + Y1;
	            float z5 = (zNMask | 1) + Z1;
	            value += (a5 * a5) * (a5 * a5) * Os2GradCoord3D(Seed2,
	                                                             i + PRIME_X, j + (yNMask & (PRIME_Y << 1)), k + (zNMask & (PRIME_Z << 1)), x5, y5, z5);
	        }
	    }
	
	    if (!bSkip9)
	    {
	        float a9 = xAFlipMask1 + zAFlipMask1 + a1;
	        if (a9 > 0)
	        {
	            float x9 = (xNMask | 1) + X1;
	            float y9 = Y1;
	            float z9 = (zNMask | 1) + Z1;
	            value += (a9 * a9) * (a9 * a9) * Os2GradCoord3D(Seed2,
	                                                             i + (xNMask & (PRIME_X * 2)), j + PRIME_Y, k + (zNMask & (PRIME_Z << 1)), x9, y9, z9);
	        }
	    }
	
	    if (!bSkipD)
	    {
	        float aD = xAFlipMask1 + yAFlipMask1 + a1;
	        if (aD > 0)
	        {
	            float xD = (xNMask | 1) + X1;
	            float yD = (yNMask | 1) + Y1;
	            float zD = Z1;
	            value += (aD * aD) * (aD * aD) * Os2GradCoord3D(Seed2,
	                                                             i + (xNMask & (PRIME_X << 1)), j + (yNMask & (PRIME_Y << 1)), k + PRIME_Z, xD, yD, zD);
	        }
	    }
	
	    return value * 9.046026385208288f;
	}
	
	

	static inline float EvalOpenSimplex2014_3D(float X, float Y, float Z, const int16* Perm, const int16* PermGradIndex3D)
	{
	
		/* Place input coordinates on simplectic honeycomb. */
		float stretchOffset = (X + Y + Z) * OS14_STRETCH_3D;
		float xs = X + stretchOffset;
		float ys = Y + stretchOffset;
		float zs = Z + stretchOffset;
		
		/* Floor to get simplectic honeycomb coordinates of rhombohedron (stretched cube) super-cell origin. */
		int32 xsb = Os14FastFloor(xs);
		int32 ysb = Os14FastFloor(ys);
		int32 zsb = Os14FastFloor(zs);
		
		/* Skew out to get actual coordinates of rhombohedron origin. We'll need these later. */
		float squishOffset = (xsb + ysb + zsb) * OS14_SQUISH_3D;
		float xb = xsb + squishOffset;
		float yb = ysb + squishOffset;
		float zb = zsb + squishOffset;
		
		/* Compute simplectic honeycomb coordinates relative to rhombohedral origin. */
		float xins = xs - xsb;
		float yins = ys - ysb;
		float zins = zs - zsb;
		
		/* Sum those together to get a value that determines which region we're in. */
		float inSum = xins + yins + zins;
	
		/* Positions relative to origin point. */
		float dx0 = X - xb;
		float dy0 = Y - yb;
		float dz0 = Z - zb;
		
		/* We'll be defining these inside the next block and using them afterwards. */
		float dx_ext0, dy_ext0, dz_ext0;
		float dx_ext1, dy_ext1, dz_ext1;
		int32 xsv_ext0, ysv_ext0, zsv_ext0;
		int32 xsv_ext1, ysv_ext1, zsv_ext1;
	
		float wins;
		int8 c, c1, c2;
		int8 aPoint, bPoint;
		float aScore, bScore;
		int32 aIsFurtherSide;
		int32 bIsFurtherSide;
		float p1, p2, p3;
		float score;
		float attn0, attn1, attn2, attn3, attn4, attn5, attn6;
		float dx1, dy1, dz1;
		float dx2, dy2, dz2;
		float dx3, dy3, dz3;
		float dx4, dy4, dz4;
		float dx5, dy5, dz5;
		float dx6, dy6, dz6;
		float attn_ext0, attn_ext1;
		
		float value = 0;
		if (inSum <= 1) { /* We're inside the tetrahedron (3-Simplex) at (0,0,0) */
			
			/* Determine which two of (0,0,1), (0,1,0), (1,0,0) are closest. */
			aPoint = 0x01;
			aScore = xins;
			bPoint = 0x02;
			bScore = yins;
			if (aScore >= bScore && zins > bScore) {
				bScore = zins;
				bPoint = 0x04;
			} else if (aScore < bScore && zins > aScore) {
				aScore = zins;
				aPoint = 0x04;
			}
			
			/* Now we determine the two lattice points not part of the tetrahedron that may contribute.
			   This depends on the closest two tetrahedral vertices, including (0,0,0) */
			wins = 1 - inSum;
			if (wins > aScore || wins > bScore) { /* (0,0,0) is one of the closest two tetrahedral vertices. */
				c = (bScore > aScore ? bPoint : aPoint); /* Our other closest vertex is the closest out of a and b. */
				
				if ((c & 0x01) == 0) {
					xsv_ext0 = xsb - 1;
					xsv_ext1 = xsb;
					dx_ext0 = dx0 + 1;
					dx_ext1 = dx0;
				} else {
					xsv_ext0 = xsv_ext1 = xsb + 1;
					dx_ext0 = dx_ext1 = dx0 - 1;
				}
	
				if ((c & 0x02) == 0) {
					ysv_ext0 = ysv_ext1 = ysb;
					dy_ext0 = dy_ext1 = dy0;
					if ((c & 0x01) == 0) {
						ysv_ext1 -= 1;
						dy_ext1 += 1;
					} else {
						ysv_ext0 -= 1;
						dy_ext0 += 1;
					}
				} else {
					ysv_ext0 = ysv_ext1 = ysb + 1;
					dy_ext0 = dy_ext1 = dy0 - 1;
				}
	
				if ((c & 0x04) == 0) {
					zsv_ext0 = zsb;
					zsv_ext1 = zsb - 1;
					dz_ext0 = dz0;
					dz_ext1 = dz0 + 1;
				} else {
					zsv_ext0 = zsv_ext1 = zsb + 1;
					dz_ext0 = dz_ext1 = dz0 - 1;
				}
			} else { /* (0,0,0) is not one of the closest two tetrahedral vertices. */
				c = (int8)(aPoint | bPoint); /* Our two extra vertices are determined by the closest two. */
				
				if ((c & 0x01) == 0) {
					xsv_ext0 = xsb;
					xsv_ext1 = xsb - 1;
					dx_ext0 = dx0 - 2 * OS14_SQUISH_3D;
					dx_ext1 = dx0 + 1 - OS14_SQUISH_3D;
				} else {
					xsv_ext0 = xsv_ext1 = xsb + 1;
					dx_ext0 = dx0 - 1 - 2 * OS14_SQUISH_3D;
					dx_ext1 = dx0 - 1 - OS14_SQUISH_3D;
				}
	
				if ((c & 0x02) == 0) {
					ysv_ext0 = ysb;
					ysv_ext1 = ysb - 1;
					dy_ext0 = dy0 - 2 * OS14_SQUISH_3D;
					dy_ext1 = dy0 + 1 - OS14_SQUISH_3D;
				} else {
					ysv_ext0 = ysv_ext1 = ysb + 1;
					dy_ext0 = dy0 - 1 - 2 * OS14_SQUISH_3D;
					dy_ext1 = dy0 - 1 - OS14_SQUISH_3D;
				}
	
				if ((c & 0x04) == 0) {
					zsv_ext0 = zsb;
					zsv_ext1 = zsb - 1;
					dz_ext0 = dz0 - 2 * OS14_SQUISH_3D;
					dz_ext1 = dz0 + 1 - OS14_SQUISH_3D;
				} else {
					zsv_ext0 = zsv_ext1 = zsb + 1;
					dz_ext0 = dz0 - 1 - 2 * OS14_SQUISH_3D;
					dz_ext1 = dz0 - 1 - OS14_SQUISH_3D;
				}
			}
	
			/* Contribution (0,0,0) */
			attn0 = 2 - dx0 * dx0 - dy0 * dy0 - dz0 * dz0;
			if (attn0 > 0) {
				attn0 *= attn0;
				value += attn0 * attn0 * Extrapolate3(Perm, PermGradIndex3D, xsb + 0, ysb + 0, zsb + 0, dx0, dy0, dz0);
			}
	
			/* Contribution (1,0,0) */
			dx1 = dx0 - 1 - OS14_SQUISH_3D;
			dy1 = dy0 - 0 - OS14_SQUISH_3D;
			dz1 = dz0 - 0 - OS14_SQUISH_3D;
			attn1 = 2 - dx1 * dx1 - dy1 * dy1 - dz1 * dz1;
			if (attn1 > 0) {
				attn1 *= attn1;
				value += attn1 * attn1 * Extrapolate3(Perm, PermGradIndex3D, xsb + 1, ysb + 0, zsb + 0, dx1, dy1, dz1);
			}
	
			/* Contribution (0,1,0) */
			dx2 = dx0 - 0 - OS14_SQUISH_3D;
			dy2 = dy0 - 1 - OS14_SQUISH_3D;
			dz2 = dz1;
			attn2 = 2 - dx2 * dx2 - dy2 * dy2 - dz2 * dz2;
			if (attn2 > 0) {
				attn2 *= attn2;
				value += attn2 * attn2 * Extrapolate3(Perm, PermGradIndex3D, xsb + 0, ysb + 1, zsb + 0, dx2, dy2, dz2);
			}
	
			/* Contribution (0,0,1) */
			dx3 = dx2;
			dy3 = dy1;
			dz3 = dz0 - 1 - OS14_SQUISH_3D;
			attn3 = 2 - dx3 * dx3 - dy3 * dy3 - dz3 * dz3;
			if (attn3 > 0) {
				attn3 *= attn3;
				value += attn3 * attn3 * Extrapolate3(Perm, PermGradIndex3D, xsb + 0, ysb + 0, zsb + 1, dx3, dy3, dz3);
			}
		} else if (inSum >= 2) { /* We're inside the tetrahedron (3-Simplex) at (1,1,1) */
		
			/* Determine which two tetrahedral vertices are the closest, out of (1,1,0), (1,0,1), (0,1,1) but not (1,1,1). */
			aPoint = 0x06;
			aScore = xins;
			bPoint = 0x05;
			bScore = yins;
			if (aScore <= bScore && zins < bScore) {
				bScore = zins;
				bPoint = 0x03;
			} else if (aScore > bScore && zins < aScore) {
				aScore = zins;
				aPoint = 0x03;
			}
			
			/* Now we determine the two lattice points not part of the tetrahedron that may contribute.
			   This depends on the closest two tetrahedral vertices, including (1,1,1) */
			wins = 3 - inSum;
			if (wins < aScore || wins < bScore) { /* (1,1,1) is one of the closest two tetrahedral vertices. */
				c = (bScore < aScore ? bPoint : aPoint); /* Our other closest vertex is the closest out of a and b. */
				
				if ((c & 0x01) != 0) {
					xsv_ext0 = xsb + 2;
					xsv_ext1 = xsb + 1;
					dx_ext0 = dx0 - 2 - 3 * OS14_SQUISH_3D;
					dx_ext1 = dx0 - 1 - 3 * OS14_SQUISH_3D;
				} else {
					xsv_ext0 = xsv_ext1 = xsb;
					dx_ext0 = dx_ext1 = dx0 - 3 * OS14_SQUISH_3D;
				}
	
				if ((c & 0x02) != 0) {
					ysv_ext0 = ysv_ext1 = ysb + 1;
					dy_ext0 = dy_ext1 = dy0 - 1 - 3 * OS14_SQUISH_3D;
					if ((c & 0x01) != 0) {
						ysv_ext1 += 1;
						dy_ext1 -= 1;
					} else {
						ysv_ext0 += 1;
						dy_ext0 -= 1;
					}
				} else {
					ysv_ext0 = ysv_ext1 = ysb;
					dy_ext0 = dy_ext1 = dy0 - 3 * OS14_SQUISH_3D;
				}
	
				if ((c & 0x04) != 0) {
					zsv_ext0 = zsb + 1;
					zsv_ext1 = zsb + 2;
					dz_ext0 = dz0 - 1 - 3 * OS14_SQUISH_3D;
					dz_ext1 = dz0 - 2 - 3 * OS14_SQUISH_3D;
				} else {
					zsv_ext0 = zsv_ext1 = zsb;
					dz_ext0 = dz_ext1 = dz0 - 3 * OS14_SQUISH_3D;
				}
			} else { /* (1,1,1) is not one of the closest two tetrahedral vertices. */
				c = (int8)(aPoint & bPoint); /* Our two extra vertices are determined by the closest two. */
				
				if ((c & 0x01) != 0) {
					xsv_ext0 = xsb + 1;
					xsv_ext1 = xsb + 2;
					dx_ext0 = dx0 - 1 - OS14_SQUISH_3D;
					dx_ext1 = dx0 - 2 - 2 * OS14_SQUISH_3D;
				} else {
					xsv_ext0 = xsv_ext1 = xsb;
					dx_ext0 = dx0 - OS14_SQUISH_3D;
					dx_ext1 = dx0 - 2 * OS14_SQUISH_3D;
				}
	
				if ((c & 0x02) != 0) {
					ysv_ext0 = ysb + 1;
					ysv_ext1 = ysb + 2;
					dy_ext0 = dy0 - 1 - OS14_SQUISH_3D;
					dy_ext1 = dy0 - 2 - 2 * OS14_SQUISH_3D;
				} else {
					ysv_ext0 = ysv_ext1 = ysb;
					dy_ext0 = dy0 - OS14_SQUISH_3D;
					dy_ext1 = dy0 - 2 * OS14_SQUISH_3D;
				}
	
				if ((c & 0x04) != 0) {
					zsv_ext0 = zsb + 1;
					zsv_ext1 = zsb + 2;
					dz_ext0 = dz0 - 1 - OS14_SQUISH_3D;
					dz_ext1 = dz0 - 2 - 2 * OS14_SQUISH_3D;
				} else {
					zsv_ext0 = zsv_ext1 = zsb;
					dz_ext0 = dz0 - OS14_SQUISH_3D;
					dz_ext1 = dz0 - 2 * OS14_SQUISH_3D;
				}
			}
			
			/* Contribution (1,1,0) */
			dx3 = dx0 - 1 - 2 * OS14_SQUISH_3D;
			dy3 = dy0 - 1 - 2 * OS14_SQUISH_3D;
			dz3 = dz0 - 0 - 2 * OS14_SQUISH_3D;
			attn3 = 2 - dx3 * dx3 - dy3 * dy3 - dz3 * dz3;
			if (attn3 > 0) {
				attn3 *= attn3;
				value += attn3 * attn3 * Extrapolate3(Perm, PermGradIndex3D, xsb + 1, ysb + 1, zsb + 0, dx3, dy3, dz3);
			}
	
			/* Contribution (1,0,1) */
			dx2 = dx3;
			dy2 = dy0 - 0 - 2 * OS14_SQUISH_3D;
			dz2 = dz0 - 1 - 2 * OS14_SQUISH_3D;
			attn2 = 2 - dx2 * dx2 - dy2 * dy2 - dz2 * dz2;
			if (attn2 > 0) {
				attn2 *= attn2;
				value += attn2 * attn2 * Extrapolate3(Perm, PermGradIndex3D, xsb + 1, ysb + 0, zsb + 1, dx2, dy2, dz2);
			}
	
			/* Contribution (0,1,1) */
			dx1 = dx0 - 0 - 2 * OS14_SQUISH_3D;
			dy1 = dy3;
			dz1 = dz2;
			attn1 = 2 - dx1 * dx1 - dy1 * dy1 - dz1 * dz1;
			if (attn1 > 0) {
				attn1 *= attn1;
				value += attn1 * attn1 * Extrapolate3(Perm, PermGradIndex3D, xsb + 0, ysb + 1, zsb + 1, dx1, dy1, dz1);
			}
	
			/* Contribution (1,1,1) */
			dx0 = dx0 - 1 - 3 * OS14_SQUISH_3D;
			dy0 = dy0 - 1 - 3 * OS14_SQUISH_3D;
			dz0 = dz0 - 1 - 3 * OS14_SQUISH_3D;
			attn0 = 2 - dx0 * dx0 - dy0 * dy0 - dz0 * dz0;
			if (attn0 > 0) {
				attn0 *= attn0;
				value += attn0 * attn0 * Extrapolate3(Perm, PermGradIndex3D, xsb + 1, ysb + 1, zsb + 1, dx0, dy0, dz0);
			}
		} else { /* We're inside the octahedron (Rectified 3-Simplex) in between.
			        Decide between point (0,0,1) and (1,1,0) as closest */
			p1 = xins + yins;
			if (p1 > 1) {
				aScore = p1 - 1;
				aPoint = 0x03;
				aIsFurtherSide = 1;
			} else {
				aScore = 1 - p1;
				aPoint = 0x04;
				aIsFurtherSide = 0;
			}
	
			/* Decide between point (0,1,0) and (1,0,1) as closest */
			p2 = xins + zins;
			if (p2 > 1) {
				bScore = p2 - 1;
				bPoint = 0x05;
				bIsFurtherSide = 1;
			} else {
				bScore = 1 - p2;
				bPoint = 0x02;
				bIsFurtherSide = 0;
			}
			
			/* The closest out of the two (1,0,0) and (0,1,1) will replace the furthest out of the two decided above, if closer. */
			p3 = yins + zins;
			if (p3 > 1) {
				score = p3 - 1;
				if (aScore <= bScore && aScore < score) {
					// aScore = score; dead store
					aPoint = 0x06;
					aIsFurtherSide = 1;
				} else if (aScore > bScore && bScore < score) {
					// bScore = score; dead store
					bPoint = 0x06;
					bIsFurtherSide = 1;
				}
			} else {
				score = 1 - p3;
				if (aScore <= bScore && aScore < score) {
					// aScore = score; dead store
					aPoint = 0x01;
					aIsFurtherSide = 0;
				} else if (aScore > bScore && bScore < score) {
					// bScore = score; dead store
					bPoint = 0x01;
					bIsFurtherSide = 0;
				}
			}
			
			/* Where each of the two closest points are determines how the extra two vertices are calculated. */
			if (aIsFurtherSide == bIsFurtherSide) {
				if (aIsFurtherSide) { /* Both closest points on (1,1,1) side */
	
					/* One of the two extra points is (1,1,1) */
					dx_ext0 = dx0 - 1 - 3 * OS14_SQUISH_3D;
					dy_ext0 = dy0 - 1 - 3 * OS14_SQUISH_3D;
					dz_ext0 = dz0 - 1 - 3 * OS14_SQUISH_3D;
					xsv_ext0 = xsb + 1;
					ysv_ext0 = ysb + 1;
					zsv_ext0 = zsb + 1;
	
					/* Other extra point is based on the shared axis. */
					c = (int8)(aPoint & bPoint);
					if ((c & 0x01) != 0) {
						dx_ext1 = dx0 - 2 - 2 * OS14_SQUISH_3D;
						dy_ext1 = dy0 - 2 * OS14_SQUISH_3D;
						dz_ext1 = dz0 - 2 * OS14_SQUISH_3D;
						xsv_ext1 = xsb + 2;
						ysv_ext1 = ysb;
						zsv_ext1 = zsb;
					} else if ((c & 0x02) != 0) {
						dx_ext1 = dx0 - 2 * OS14_SQUISH_3D;
						dy_ext1 = dy0 - 2 - 2 * OS14_SQUISH_3D;
						dz_ext1 = dz0 - 2 * OS14_SQUISH_3D;
						xsv_ext1 = xsb;
						ysv_ext1 = ysb + 2;
						zsv_ext1 = zsb;
					} else {
						dx_ext1 = dx0 - 2 * OS14_SQUISH_3D;
						dy_ext1 = dy0 - 2 * OS14_SQUISH_3D;
						dz_ext1 = dz0 - 2 - 2 * OS14_SQUISH_3D;
						xsv_ext1 = xsb;
						ysv_ext1 = ysb;
						zsv_ext1 = zsb + 2;
					}
				} else { /* Both closest points on (0,0,0) side */
	
					/* One of the two extra points is (0,0,0) */
					dx_ext0 = dx0;
					dy_ext0 = dy0;
					dz_ext0 = dz0;
					xsv_ext0 = xsb;
					ysv_ext0 = ysb;
					zsv_ext0 = zsb;
	
					/* Other extra point is based on the omitted axis. */
					c = (int8)(aPoint | bPoint);
					if ((c & 0x01) == 0) {
						dx_ext1 = dx0 + 1 - OS14_SQUISH_3D;
						dy_ext1 = dy0 - 1 - OS14_SQUISH_3D;
						dz_ext1 = dz0 - 1 - OS14_SQUISH_3D;
						xsv_ext1 = xsb - 1;
						ysv_ext1 = ysb + 1;
						zsv_ext1 = zsb + 1;
					} else if ((c & 0x02) == 0) {
						dx_ext1 = dx0 - 1 - OS14_SQUISH_3D;
						dy_ext1 = dy0 + 1 - OS14_SQUISH_3D;
						dz_ext1 = dz0 - 1 - OS14_SQUISH_3D;
						xsv_ext1 = xsb + 1;
						ysv_ext1 = ysb - 1;
						zsv_ext1 = zsb + 1;
					} else {
						dx_ext1 = dx0 - 1 - OS14_SQUISH_3D;
						dy_ext1 = dy0 - 1 - OS14_SQUISH_3D;
						dz_ext1 = dz0 + 1 - OS14_SQUISH_3D;
						xsv_ext1 = xsb + 1;
						ysv_ext1 = ysb + 1;
						zsv_ext1 = zsb - 1;
					}
				}
			} else { /* One point on (0,0,0) side, one point on (1,1,1) side */
				if (aIsFurtherSide) {
					c1 = aPoint;
					c2 = bPoint;
				} else {
					c1 = bPoint;
					c2 = aPoint;
				}
	
				/* One contribution is a permutation of (1,1,-1) */
				if ((c1 & 0x01) == 0) {
					dx_ext0 = dx0 + 1 - OS14_SQUISH_3D;
					dy_ext0 = dy0 - 1 - OS14_SQUISH_3D;
					dz_ext0 = dz0 - 1 - OS14_SQUISH_3D;
					xsv_ext0 = xsb - 1;
					ysv_ext0 = ysb + 1;
					zsv_ext0 = zsb + 1;
				} else if ((c1 & 0x02) == 0) {
					dx_ext0 = dx0 - 1 - OS14_SQUISH_3D;
					dy_ext0 = dy0 + 1 - OS14_SQUISH_3D;
					dz_ext0 = dz0 - 1 - OS14_SQUISH_3D;
					xsv_ext0 = xsb + 1;
					ysv_ext0 = ysb - 1;
					zsv_ext0 = zsb + 1;
				} else {
					dx_ext0 = dx0 - 1 - OS14_SQUISH_3D;
					dy_ext0 = dy0 - 1 - OS14_SQUISH_3D;
					dz_ext0 = dz0 + 1 - OS14_SQUISH_3D;
					xsv_ext0 = xsb + 1;
					ysv_ext0 = ysb + 1;
					zsv_ext0 = zsb - 1;
				}
	
				/* One contribution is a permutation of (0,0,2) */
				dx_ext1 = dx0 - 2 * OS14_SQUISH_3D;
				dy_ext1 = dy0 - 2 * OS14_SQUISH_3D;
				dz_ext1 = dz0 - 2 * OS14_SQUISH_3D;
				xsv_ext1 = xsb;
				ysv_ext1 = ysb;
				zsv_ext1 = zsb;
				if ((c2 & 0x01) != 0) {
					dx_ext1 -= 2;
					xsv_ext1 += 2;
				} else if ((c2 & 0x02) != 0) {
					dy_ext1 -= 2;
					ysv_ext1 += 2;
				} else {
					dz_ext1 -= 2;
					zsv_ext1 += 2;
				}
			}
	
			/* Contribution (1,0,0) */
			dx1 = dx0 - 1 - OS14_SQUISH_3D;
			dy1 = dy0 - 0 - OS14_SQUISH_3D;
			dz1 = dz0 - 0 - OS14_SQUISH_3D;
			attn1 = 2 - dx1 * dx1 - dy1 * dy1 - dz1 * dz1;
			if (attn1 > 0) {
				attn1 *= attn1;
				value += attn1 * attn1 * Extrapolate3(Perm, PermGradIndex3D, xsb + 1, ysb + 0, zsb + 0, dx1, dy1, dz1);
			}
	
			/* Contribution (0,1,0) */
			dx2 = dx0 - 0 - OS14_SQUISH_3D;
			dy2 = dy0 - 1 - OS14_SQUISH_3D;
			dz2 = dz1;
			attn2 = 2 - dx2 * dx2 - dy2 * dy2 - dz2 * dz2;
			if (attn2 > 0) {
				attn2 *= attn2;
				value += attn2 * attn2 * Extrapolate3(Perm, PermGradIndex3D, xsb + 0, ysb + 1, zsb + 0, dx2, dy2, dz2);
			}
	
			/* Contribution (0,0,1) */
			dx3 = dx2;
			dy3 = dy1;
			dz3 = dz0 - 1 - OS14_SQUISH_3D;
			attn3 = 2 - dx3 * dx3 - dy3 * dy3 - dz3 * dz3;
			if (attn3 > 0) {
				attn3 *= attn3;
				value += attn3 * attn3 * Extrapolate3(Perm, PermGradIndex3D, xsb + 0, ysb + 0, zsb + 1, dx3, dy3, dz3);
			}
	
			/* Contribution (1,1,0) */
			dx4 = dx0 - 1 - 2 * OS14_SQUISH_3D;
			dy4 = dy0 - 1 - 2 * OS14_SQUISH_3D;
			dz4 = dz0 - 0 - 2 * OS14_SQUISH_3D;
			attn4 = 2 - dx4 * dx4 - dy4 * dy4 - dz4 * dz4;
			if (attn4 > 0) {
				attn4 *= attn4;
				value += attn4 * attn4 * Extrapolate3(Perm, PermGradIndex3D, xsb + 1, ysb + 1, zsb + 0, dx4, dy4, dz4);
			}
	
			/* Contribution (1,0,1) */
			dx5 = dx4;
			dy5 = dy0 - 0 - 2 * OS14_SQUISH_3D;
			dz5 = dz0 - 1 - 2 * OS14_SQUISH_3D;
			attn5 = 2 - dx5 * dx5 - dy5 * dy5 - dz5 * dz5;
			if (attn5 > 0) {
				attn5 *= attn5;
				value += attn5 * attn5 * Extrapolate3(Perm, PermGradIndex3D, xsb + 1, ysb + 0, zsb + 1, dx5, dy5, dz5);
			}
	
			/* Contribution (0,1,1) */
			dx6 = dx0 - 0 - 2 * OS14_SQUISH_3D;
			dy6 = dy4;
			dz6 = dz5;
			attn6 = 2 - dx6 * dx6 - dy6 * dy6 - dz6 * dz6;
			if (attn6 > 0) {
				attn6 *= attn6;
				value += attn6 * attn6 * Extrapolate3(Perm, PermGradIndex3D, xsb + 0, ysb + 1, zsb + 1, dx6, dy6, dz6);
			}
		}
	
		/* First extra vertex */
		attn_ext0 = 2 - dx_ext0 * dx_ext0 - dy_ext0 * dy_ext0 - dz_ext0 * dz_ext0;
		if (attn_ext0 > 0)
		{
			attn_ext0 *= attn_ext0;
			value += attn_ext0 * attn_ext0 * Extrapolate3(Perm, PermGradIndex3D, xsv_ext0, ysv_ext0, zsv_ext0, dx_ext0, dy_ext0, dz_ext0);
		}
	
		/* Second extra vertex */
		attn_ext1 = 2 - dx_ext1 * dx_ext1 - dy_ext1 * dy_ext1 - dz_ext1 * dz_ext1;
		if (attn_ext1 > 0)
		{
			attn_ext1 *= attn_ext1;
			value += attn_ext1 * attn_ext1 * Extrapolate3(Perm, PermGradIndex3D, xsv_ext1, ysv_ext1, zsv_ext1, dx_ext1, dy_ext1, dz_ext1);
		}
		
		return value / OS14_NORM_3D;
	}
		
	
} // OpenSimplexDetail

	FORCEINLINE void OpenSimplex2Rotate3D(float& X, float& Y, float& Z)
	{
		constexpr float R3 = 2.f / 3.f;
		const float r = (X + Y + Z) * R3;
		X = r - X;
		Y = r - Y;
		Z = r - Z;
	}

	inline float OpenSimplex2F_3D(float X, float Y, float Z, int32 Seed = 1337)
	{
		OpenSimplex2Rotate3D(X, Y, Z);
		return OpenSimplexDetail::SingleOpenSimplex2F3D(Seed, X, Y, Z);
	}

	inline float OpenSimplex2S_3D(float X, float Y, float Z, int32 Seed = 1337)
	{
		OpenSimplex2Rotate3D(X, Y, Z);
		return OpenSimplexDetail::SingleOpenSimplex2S3D(Seed, X, Y, Z);
	}

	inline float OpenSimplex3D(float X, float Y, float Z, int32 Seed = 1337)
	{
		const int16* Perm = nullptr;
		const int16* PermGrad = nullptr;
		OpenSimplexDetail::EnsureOpenSimplex2014Perm(Seed, Perm, PermGrad);
		return OpenSimplexDetail::EvalOpenSimplex2014_3D(X, Y, Z, Perm, PermGrad);
	}
}
