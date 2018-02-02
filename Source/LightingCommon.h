#pragma once

#include "Utils.h"

#include <Rush/MathTypes.h>
#include <Rush/UtilTuple.h>

#include <algorithm>
#include <cmath>
#include <vector>

enum class LightingMode
{
	Clustered,
	Hybrid,
	Tree,

	count
};

struct alignas(16) LightSource
{
	Vec3  position;
	float attenuationEnd;
	Vec3  intensity;
	float padding1;
};

struct AnimatedLightSource : public LightSource
{
	Vec3 originalPosition;
	Vec3 movementDirection;
};

struct LightTileDepthExtents
{
	u8 sliceMin;
	u8 sliceMax;
};

inline u32 getSliceCount(const LightTileDepthExtents& extents) { return 1 + extents.sliceMax - extents.sliceMin; }

struct alignas(16) LightDepthInterval
{
	float                 center;
	float                 radius;
	u16                   lightIndex;
	LightTileDepthExtents depthExtents;
};

static_assert(sizeof(LightDepthInterval) == 16, "LightDepthInterval must be exactly 16 bytes");

struct alignas(16) LightTileScreenSpaceExtents
{
	Box2    screenSpaceBox;
	Tuple2u tileMin;
	Tuple2u tileMax;
};

struct alignas(16) TileFrustumParameters
{
	Vec4 planes[4];
	Vec4 cornerRays[4];
};

inline u32 getCellCount(const LightTileScreenSpaceExtents& extents)
{
	u32 x = 1 + extents.tileMax.x - extents.tileMin.x;
	u32 y = 1 + extents.tileMax.y - extents.tileMin.y;
	return x * y;
}

struct CommonLightBuildParams
{
	Tuple2u resolution = Tuple2u({1, 1});
	u32     tileSize   = 1;

	u32                    sliceCount            = 16;
	static constexpr float minSliceDepth         = 5.0f;
	float                  maxSliceDepth         = 500.0f;
	bool                   useExponentialSlices  = false;
	bool                   useTileFrustumCulling = true;

	bool calculateTileLightCount = true;
};

inline u32 asUint(float f)
{
	u32 u;
	std::memcpy(&u, &f, 4);
	return u;
}

inline float asFloat(u32 u)
{
	float f;
	std::memcpy(&f, &u, 4);
	return f;
}

inline int log2fi(float f)
{
	u32 u = asUint(f);
	int e = int((u >> 23) & 0xFF) - 127;
	return e;
}

inline u32 computeLinearSliceIndex(float depth, float maxDepth, u32 sliceCount)
{
	float t = sliceCount * depth / maxDepth;
	return (u32)min(t, (float)(sliceCount - 1));
}

inline float computeLinearSliceDepth(u32 sliceIndex, float maxDepth, u32 sliceCount)
{
	return sliceIndex * (maxDepth / sliceCount);
}

inline u32 computeExponentialSliceIndexLog(float logDepth, float logMinDepth, float logMaxDepth, u32 sliceCount)
{
	float scale = 1.0f / (logMaxDepth - logMinDepth) * (sliceCount - 1.0f);
	float bias  = 1.0f - logMinDepth * scale;
	u32   slice = u32(max(logDepth * scale + bias, 0.0f));
	return min(slice, sliceCount - 1);
}

inline u32 computeExponentialSliceIndex(float depth, float minDepth, float maxDepth, u32 sliceCount)
{
	static const float logMinDepth = log2f(minDepth);
	float              logDepth    = log2f(depth);
	float              logMaxDepth = log2f(maxDepth);
	return computeExponentialSliceIndexLog(logDepth, logMinDepth, logMaxDepth, sliceCount);
}

inline float computeExponentialSliceDepth(u32 sliceIndex, float maxDepth, u32 sliceCount)
{
	const float min_depth = log2f(5.0f);
	const float max_depth = log2f(maxDepth);
	const float scale     = 1.0f / (max_depth - min_depth) * (sliceCount - 1.0f);
	const float bias      = 1.0f - min_depth * scale;
	float       depth     = exp2f(((float)sliceIndex - bias) / scale);
	return depth;
}

inline Vec3 transformPoint(const Mat4& m, const Vec3& p)
{
	__m128 X = _mm_mul_ps(_mm_set1_ps(p.x), _mm_loadu_ps(&m.rows[0].x));
	__m128 Y = _mm_mul_ps(_mm_set1_ps(p.y), _mm_loadu_ps(&m.rows[1].x));
	__m128 Z = _mm_mul_ps(_mm_set1_ps(p.z), _mm_loadu_ps(&m.rows[2].x));

	__m128 r = _mm_loadu_ps(&m.rows[3].x);
	r        = _mm_add_ps(r, X);
	r        = _mm_add_ps(r, Y);
	r        = _mm_add_ps(r, Z);

	alignas(16) Vec4 res;
	_mm_store_ps(&res.x, r);

	return res.xyz();
}

inline Vec4 projectPoint(const Mat4& m, const Vec3& p)
{
	__m128 X = _mm_mul_ps(_mm_set1_ps(p.x), _mm_loadu_ps(&m.rows[0].x));
	__m128 Y = _mm_mul_ps(_mm_set1_ps(p.y), _mm_loadu_ps(&m.rows[1].x));
	__m128 Z = _mm_mul_ps(_mm_set1_ps(p.z), _mm_loadu_ps(&m.rows[2].x));

	__m128 r = _mm_loadu_ps(&m.rows[3].x);
	r        = _mm_add_ps(r, X);
	r        = _mm_add_ps(r, Y);
	r        = _mm_add_ps(r, Z);

	__m128 w = _mm_shuffle_ps(r, r, _MM_SHUFFLE(3, 3, 3, 3));
	r        = _mm_mul_ps(r, _mm_rcp_ps(w));

	alignas(16) Vec4 res;
	_mm_store_ps(&res.x, r);

	return res;
}

inline bool testRaySphere(const Vec3& ray, const Vec4& sphere)
{
	float r2 = sphere.w * sphere.w;
	Vec3  a  = ray * dot(sphere.xyz(), ray);
	Vec3  b  = sphere.xyz() - a;
	float d2 = dot(b, b);
	return d2 < r2;
}

inline bool testUnnormRaySphere(const Vec3& ray, const Vec4& sphere)
{
	float r2 = sphere.w * sphere.w;
	Vec3  a  = ray * dot(sphere.xyz(), ray) / dot(ray, ray);
	Vec3  b  = sphere.xyz() - a;
	float d2 = dot(b, b);
	return d2 < r2;
}

inline bool testTileFrustumSphereFastButInaccurate(const Vec2& cameraFrustumTopLeft,
    const Vec2&                                                tileStep,
    const Vec2&                                                tileTopLeft,
    const Vec2&                                                tileSpaceSphereCenter,
    const Vec4&                                                sphere)
{
	const Vec2& tl = tileTopLeft;
	const Vec2  br = tl + Vec2(1.0);

	u32 mask = 0;
	mask |= tileSpaceSphereCenter.x < tl.x ? (1 << 0) : 0;
	mask |= tileSpaceSphereCenter.y < tl.y ? (1 << 1) : 0;
	mask |= tileSpaceSphereCenter.x > br.x ? (1 << 2) : 0;
	mask |= tileSpaceSphereCenter.y > br.y ? (1 << 3) : 0;

	Vec2 delta;
	delta.x = float((mask >> 2) & 1);
	delta.y = float((mask >> 3) & 1);

	Vec2 rayDirXY = cameraFrustumTopLeft + (tileTopLeft + delta) * tileStep;
	Vec3 rayDir   = Vec3(rayDirXY.x, rayDirXY.y, 1.0f);

	if (bitCount(mask) == 2 && !testUnnormRaySphere(rayDir, sphere))
	{
		return false;
	}
	else
	{
		return true;
	}
}

inline Vec3 makeVec3(const Vec2 xy, float z) { return Vec3(xy.x, xy.y, z); }

inline float dotXZ(const Vec3& a, const Vec3 b) { return a.x * b.x + a.z * b.z; }

inline float dotYZ(const Vec3& a, const Vec3 b) { return a.y * b.y + a.z * b.z; }

inline bool testTileFrustumSphere(
    const Vec3 frustumCornerRays[4], // unnormalized vectors to frustum corners (TL, TR, BL, BR)
    const Vec3 frustumSidePlanes[4], // left, top, right, bottom
    const Vec4 sphere)
{
	Vec3 cornerRay;

	Vec3  sphereCenter = sphere.xyz();
	float sphereRadius = sphere.w;

	float dp[4];

	dp[0] = dotXZ(frustumSidePlanes[0], sphereCenter);
	dp[1] = dotYZ(frustumSidePlanes[1], sphereCenter);
	dp[2] = dotXZ(frustumSidePlanes[2], sphereCenter);
	dp[3] = dotYZ(frustumSidePlanes[3], sphereCenter);

	float minDotA = min(min(dp[0], dp[1]), min(dp[2], dp[3]));
	float minDotB;

	if (minDotA == dp[0]) // sphere is on the left, check top and bottom planes
	{
		Vec3 centerProjectedToMinPlane = sphereCenter;
		centerProjectedToMinPlane.x -= minDotA * frustumSidePlanes[0].x;
		centerProjectedToMinPlane.z -= minDotA * frustumSidePlanes[0].z;
		float dp0 = dotYZ(frustumSidePlanes[1], centerProjectedToMinPlane);
		float dp1 = dotYZ(frustumSidePlanes[3], centerProjectedToMinPlane);
		minDotB   = min(dp0, dp1);
		cornerRay = minDotB == dp0 ? frustumCornerRays[0] : frustumCornerRays[2];
	}
	else if (minDotA == dp[1]) // sphere is on the top, check left and right
	{
		Vec3 centerProjectedToMinPlane = sphereCenter;
		centerProjectedToMinPlane.y -= minDotA * frustumSidePlanes[1].y;
		centerProjectedToMinPlane.z -= minDotA * frustumSidePlanes[1].z;
		float dp0 = dotXZ(frustumSidePlanes[0], centerProjectedToMinPlane);
		float dp1 = dotXZ(frustumSidePlanes[2], centerProjectedToMinPlane);
		minDotB   = min(dp0, dp1);
		cornerRay = minDotB == dp0 ? frustumCornerRays[0] : frustumCornerRays[1];
	}
	else if (minDotA == dp[2]) // sphere is on the right, check top and bottom
	{
		Vec3 centerProjectedToMinPlane = sphereCenter;
		centerProjectedToMinPlane.x -= minDotA * frustumSidePlanes[2].x;
		centerProjectedToMinPlane.z -= minDotA * frustumSidePlanes[2].z;
		float dp0 = dotYZ(frustumSidePlanes[1], centerProjectedToMinPlane);
		float dp1 = dotYZ(frustumSidePlanes[3], centerProjectedToMinPlane);
		minDotB   = min(dp0, dp1);
		cornerRay = minDotB == dp0 ? frustumCornerRays[1] : frustumCornerRays[3];
	}
	else // sphere is on the bottom, check left and right
	{
		Vec3 centerProjectedToMinPlane = sphereCenter;
		centerProjectedToMinPlane.y -= minDotA * frustumSidePlanes[3].y;
		centerProjectedToMinPlane.z -= minDotA * frustumSidePlanes[3].z;
		float dp0 = dotXZ(frustumSidePlanes[0], centerProjectedToMinPlane);
		float dp1 = dotXZ(frustumSidePlanes[2], centerProjectedToMinPlane);
		minDotB   = min(dp0, dp1);
		cornerRay = minDotB == dp0 ? frustumCornerRays[2] : frustumCornerRays[3];
	}

	if (minDotB > 0)
	{
		// Sphere center is not outside of the corner, so single plane test is enough
		return -minDotA < sphereRadius;
	}

	// Sphere center is outside of the corner, so need to test the ray that passes from origin through that corner

	float r2 = sphere.w * sphere.w;
	Vec3  a  = cornerRay * max(0.0f, dot(sphere.xyz(), cornerRay)) / dot(cornerRay, cornerRay);
	Vec3  b  = sphere.xyz() - a;
	float d2 = dot(b, b);

	return d2 < r2;
}

inline void computeTileFrustumParameters(const Vec2& cameraTopLeftCornerRay,
    const Vec2&                                      tileStep,
    const Vec2&                                      tileTopLeft,
    Vec3 outFrustumCornerRays[4], // unnormalized vectors to frustum corners (TL, TR, BL, BR)
    Vec3 outFrustumSidePlanes[4]) // left, top, right, bottom
{
	outFrustumCornerRays[0] = makeVec3(cameraTopLeftCornerRay + Vec2(tileStep * Vec2(tileTopLeft + Vec2(0, 0))), 1);
	outFrustumCornerRays[1] = makeVec3(cameraTopLeftCornerRay + Vec2(tileStep * Vec2(tileTopLeft + Vec2(1, 0))), 1);
	outFrustumCornerRays[2] = makeVec3(cameraTopLeftCornerRay + Vec2(tileStep * Vec2(tileTopLeft + Vec2(0, 1))), 1);
	outFrustumCornerRays[3] = makeVec3(cameraTopLeftCornerRay + Vec2(tileStep * Vec2(tileTopLeft + Vec2(1, 1))), 1);

	outFrustumSidePlanes[0] = normalize(cross(outFrustumCornerRays[0], outFrustumCornerRays[2]));
	outFrustumSidePlanes[1] = normalize(cross(outFrustumCornerRays[1], outFrustumCornerRays[0]));
	outFrustumSidePlanes[2] = normalize(cross(outFrustumCornerRays[3], outFrustumCornerRays[1]));
	outFrustumSidePlanes[3] = normalize(cross(outFrustumCornerRays[2], outFrustumCornerRays[3]));
}

inline bool testTileFrustumSphere(
    const Vec2& cameraFrustumTopLeft, const Vec2& tileStep, const Vec2& tileTopLeft, const Vec4 sphere)
{
	Vec3 corners[4];
	Vec3 frustumSidePlanes[4];
	computeTileFrustumParameters(cameraFrustumTopLeft, tileStep, tileTopLeft, corners, frustumSidePlanes);
	return testTileFrustumSphere(corners, frustumSidePlanes, sphere);
}

struct DepthExtentsCalculator
{
	DepthExtentsCalculator(const CommonLightBuildParams& buildParams)
	: maxSliceDepth(buildParams.maxSliceDepth)
	, sliceCount(buildParams.sliceCount)
	, logMinSliceDepth(log2f(buildParams.minSliceDepth))
	, logMaxSliceDepth(log2f(buildParams.maxSliceDepth))
	, useExponentialSlices(buildParams.useExponentialSlices)
	{
		for (u32 i = 1; i < sliceCount; ++i)
		{
			if (useExponentialSlices)
			{
				float d = computeExponentialSliceDepth(i, maxSliceDepth, sliceCount);
				sliceDistances.push_back(d);
			}
			else
			{
				float d = computeLinearSliceDepth(i, maxSliceDepth, sliceCount);
				sliceDistances.push_back(d);
			}
		}
		sliceDistances.push_back(FLT_MAX);
	}

	inline LightTileDepthExtents calculateDepthExtents(const LightDepthInterval& interval) const
	{
		float depthMin = max(interval.center - interval.radius, 0.0f);
		float depthMax = min(interval.center + interval.radius, maxSliceDepth);

		LightTileDepthExtents result;

		if (useExponentialSlices)
		{
#if 0
			float logDepthMin = log2f(depthMin);
			float logDepthMax = log2f(depthMax);
			result.sliceMin = computeExponentialSliceIndexLog(logDepthMin, logMinSliceDepth, logMaxSliceDepth, sliceCount);
			result.sliceMax = computeExponentialSliceIndexLog(logDepthMax, logMinSliceDepth, logMaxSliceDepth, sliceCount);
#else
			auto it1        = std::lower_bound(sliceDistances.begin(), sliceDistances.end(), depthMin);
			auto it2        = std::lower_bound(it1, sliceDistances.end(), depthMax);
			result.sliceMin = (u32)std::distance(sliceDistances.begin(), it1);
			result.sliceMax = (u32)std::distance(sliceDistances.begin(), it2);
#endif
		}
		else
		{
			result.sliceMin = computeLinearSliceIndex(depthMin, maxSliceDepth, sliceCount);
			result.sliceMax = computeLinearSliceIndex(depthMax, maxSliceDepth, sliceCount);
		}

		return result;
	};

	std::vector<float> sliceDistances;

	const float maxSliceDepth;
	const u32   sliceCount;

	const float logMinSliceDepth;
	const float logMaxSliceDepth;

	const bool useExponentialSlices;
};

struct TileFrustumCache
{
	void build(float fov, float aspect, u32 tileSize, u32 tileCountX, u32 tileCountY, u32 resolutionX);

	std::vector<Vec3> m_corners;
	std::vector<Vec3> m_planes;

	float m_tileFrustumStepSize = -1;
	Vec2  m_tileStep            = -1;
	u32   m_tileSize            = 0;
	u32   m_tileCountX          = 0;
	u32   m_tileCountY          = 0;
	u32   m_resolutionX         = 0;
};

u32 performLightBinning(const DepthExtentsCalculator& depthExtentsCalculator,
    const Mat4&                                       matProjScreenSpace, // view space to sceen space transform
    const float cameraNearZ, int tileSize, int tileCountX, int tileCountY, const std::vector<LightSource>& lights,
    AlignedArray<LightDepthInterval>& inOutCulledLights, LightTileScreenSpaceExtents* outLightScreenSpaceExtents,
    u16* outCellLightCount);

// Returns number of lights that passed frustum culling
u32 performLightCulling(
    const Frustum& frustum, const std::vector<LightSource>& viewSpaceLights, AlignedArray<u32>& outIndices);

void computeLightDepthIntervals(const std::vector<LightSource>& viewSpaceLights,
    const AlignedArray<u32>&                                    lightIndices,
    AlignedArray<LightDepthInterval>&                           outLightIntervals);

u32 performLightCullingAndComputeDepthIntervals(const Frustum& frustum,
    const std::vector<LightSource>&                            viewSpaceLights,
    AlignedArray<u32>&                                         outIndices,
    AlignedArray<LightDepthInterval>&                          outLightIntervals);

const char* toString(LightingMode mode);
