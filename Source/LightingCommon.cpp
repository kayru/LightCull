#include "LightingCommon.h"
#include "Utils.h"

inline void getBoundsForAxis(const Vec3& axis, const Vec3& C, float r, float nearZ, Vec3& L, Vec3& U)
{
	const Vec2 c(dot(axis, C), C.z);

	Vec2 bounds[2]; // In the a-z reference frame

	const float radiusSquared = sqr(r);

	const float tSquared = dot(c, c) - radiusSquared;

	const bool cameraInsideSphere = (tSquared <= 0);

	// (cos, sin) of angle theta between c and a tangent vector
	Vec2 v = cameraInsideSphere ? Vec2(0.0f, 0.0f) : Vec2(sqrtf(tSquared), r) / c.length();

	// Does the near plane intersect the sphere?
	const bool clipSphere = (C.z - r <= nearZ);

	// Square root of the discriminant;
	// NaN (and unused) if the camera is in the sphere
	float k = sqrtf(radiusSquared - sqr(C.z - nearZ));

	for (int i = 0; i < 2; ++i)
	{
		if (!cameraInsideSphere)
		{
			Mat2 m{
			    Vec2(v.x, -v.y),
			    Vec2(v.y, v.x),
			};

			bounds[i] = m * c * v.x;
		}

		const bool clipBound = cameraInsideSphere || (bounds[i].y < nearZ);
		if (clipSphere && clipBound)
		{
			bounds[i] = Vec2(c.x + k, nearZ);
		}

		// Set up for the lower bound
		v.y = -v.y;
		k   = -k;
	}

	// Transform back to camera space
	L   = bounds[1].x * axis;
	L.z = bounds[1].y;

	U   = bounds[0].x * axis;
	U.z = bounds[0].y;
}

inline Box2 computeProjectedBoundingBox(const Mat4& matProj, const Vec3& C, float r, float nearZ)
{
	// Based on http://jcgt.org/published/0002/02/05/paper.pdf

	Vec3 right, left, top, bottom;

	getBoundsForAxis(Vec3(1, 0, 0), C, r, nearZ, left, right);
	getBoundsForAxis(Vec3(0, 1, 0), C, r, nearZ, top, bottom);

	return Box2(projectPoint(matProj, left).x, projectPoint(matProj, bottom).y, projectPoint(matProj, right).x,
	    projectPoint(matProj, top).y);
}

inline Box2 computeProjectedBoundingBoxFast(const Mat4& matProj, const Vec3& C, float r, float nearZ)
{
	Vec3 right, left, top, bottom;

	left  = C - Vec3(r, 0, 0);
	right = C + Vec3(r, 0, 0);

	top    = C - Vec3(0, r, 0);
	bottom = C + Vec3(0, r, 0);

	return Box2(projectPoint(matProj, left).x, projectPoint(matProj, bottom).y, projectPoint(matProj, right).x,
	    projectPoint(matProj, top).y);
}

u32 performLightBinning(const DepthExtentsCalculator& depthExtentsCalculator, const Mat4& matProjScreenSpace,
    const float cameraNearZ, int tileSize, int tileCountX, int tileCountY, const std::vector<LightSource>& lights,
    AlignedArray<LightDepthInterval>& inOutCulledLights, LightTileScreenSpaceExtents* outLightScreenSpaceExtents,
    u16* outCellLightCount)
{
	const u32 tilesPerSlice = tileCountX * tileCountY;

#if USE_PARALLEL_ALGORITHMS
	parallelForEach(inOutCulledLights.begin(), inOutCulledLights.end(),
	    [&](LightDepthInterval& interval)
#else
	for (LightDepthInterval& interval : inOutCulledLights)
#endif
	    {
		    const LightSource&           light   = lights[interval.lightIndex];
		    LightTileScreenSpaceExtents& extents = outLightScreenSpaceExtents[interval.lightIndex];

		    extents.screenSpaceBox =
		        computeProjectedBoundingBox(matProjScreenSpace, light.position, light.attenuationEnd, cameraNearZ);

		    extents.tileMin.x = min(max((int)extents.screenSpaceBox.m_min.x / tileSize, 0), tileCountX - 1);
		    extents.tileMin.y = min(max((int)extents.screenSpaceBox.m_min.y / tileSize, 0), tileCountY - 1);
		    extents.tileMax.x = min(max((int)extents.screenSpaceBox.m_max.x / tileSize, 0), tileCountX - 1);
		    extents.tileMax.y = min(max((int)extents.screenSpaceBox.m_max.y / tileSize, 0), tileCountY - 1);

		    // TODO: implement tight light culling instead of simply considering view space bounding box

		    interval.depthExtents = depthExtentsCalculator.calculateDepthExtents(interval);

#if USE_PARALLEL_ALGORITHMS
	    });
#else
	}
#endif

	u32 totalLightCellCount = 0;
	for (LightDepthInterval& interval : inOutCulledLights)
	{
		const LightTileScreenSpaceExtents& screenSpaceExtents = outLightScreenSpaceExtents[interval.lightIndex];
		for (u32 z = interval.depthExtents.sliceMin; z <= interval.depthExtents.sliceMax; ++z)
		{
			for (u32 y = screenSpaceExtents.tileMin.y; y <= screenSpaceExtents.tileMax.y; ++y)
			{
				for (u32 x = screenSpaceExtents.tileMin.x; x <= screenSpaceExtents.tileMax.x; ++x)
				{
					u32 tileIndex = x + y * tileCountX;
					u32 cellIndex = tileIndex + z * tilesPerSlice;
					outCellLightCount[cellIndex]++;
				}
			}
		}

		u32 lightCellCount = getSliceCount(interval.depthExtents) * getCellCount(screenSpaceExtents);
		totalLightCellCount += lightCellCount;
	}

	return totalLightCellCount;
}

u32 performLightCulling(
    const Frustum& frustum, const std::vector<LightSource>& viewSpaceLights, AlignedArray<u32>& outIndices)
{
	outIndices.resize(viewSpaceLights.size());

	u32 outputCount = 0;
	for (size_t lightIndex = 0; lightIndex < viewSpaceLights.size(); ++lightIndex)
	{
		const LightSource& light = viewSpaceLights[lightIndex];
		if (frustum.intersectSphereConservative(light.position, light.attenuationEnd))
		{
			outIndices[outputCount] = (u16)lightIndex;
			++outputCount;
		}
	}

	outIndices.m_count = outputCount;

	return (u32)outIndices.m_count;
}

void computeLightDepthIntervals(const std::vector<LightSource>& lights,
    const AlignedArray<u32>&                                    lightIndices,
    AlignedArray<LightDepthInterval>&                           outLightIntervals)
{
	outLightIntervals.resize(lightIndices.size());

	for (size_t i = 0; i < lightIndices.size(); ++i)
	{
		int                 lightIndex = lightIndices[i];
		const LightSource&  light      = lights[lightIndex];
		LightDepthInterval& interval   = outLightIntervals[i];
		interval.radius                = light.attenuationEnd;
		interval.center                = light.position.z;
		interval.lightIndex            = (u16)lightIndex;
	}
}

u32 performLightCullingAndComputeDepthIntervals(const Frustum& frustum,
    const std::vector<LightSource>&                            viewSpaceLights,
    AlignedArray<u32>&                                         outIndices,
    AlignedArray<LightDepthInterval>&                          outLightIntervals)
{
	outIndices.resize(viewSpaceLights.size());
	outLightIntervals.resize(viewSpaceLights.size());

	u32 outputCount = 0;

	parallelForEach(viewSpaceLights.begin(), viewSpaceLights.end(), [&](const LightSource& light) {
		if (frustum.intersectSphereConservative(light.position, light.attenuationEnd))
		{
			u64 lightIndex = &light - viewSpaceLights.data();

			u32 writeIndex = interlockedIncrement(outputCount) - 1;

			outIndices[writeIndex] = (u16)lightIndex;

			LightDepthInterval& interval = outLightIntervals[writeIndex];
			interval.radius              = light.attenuationEnd;
			interval.center              = light.position.z;
			interval.lightIndex          = (u16)lightIndex;
		}
	});

	outIndices.m_count        = outputCount;
	outLightIntervals.m_count = outputCount;

	return outputCount;
}

const char* toString(LightingMode mode)
{
	switch (mode)
	{
	default: return "Unknown";
	case LightingMode::Hybrid: return "Hybrid";
	case LightingMode::Clustered: return "Clustered";
	case LightingMode::Tree: return "Tree";
	}
}

void TileFrustumCache::build(float fov, float aspect, u32 tileSize, u32 tileCountX, u32 tileCountY, u32 resolutionX)
{
	float yHeight                    = tan(fov / 2) * 2;
	float xWidth                     = yHeight * aspect;
	Vec2  cameraFrustumTopLeftCorner = Vec2(-xWidth / 2, yHeight / 2);
	float tileFrustumStepSize        = xWidth * float(tileSize) / float(resolutionX);
	Vec2  tileStep                   = Vec2(tileFrustumStepSize, -tileFrustumStepSize);

	if (m_tileFrustumStepSize == tileFrustumStepSize && m_tileStep == tileStep && m_tileSize == tileSize &&
	    m_tileCountX == tileCountX && m_tileCountY == tileCountY && m_resolutionX == resolutionX)
	{
		return;
	}

	m_tileFrustumStepSize = tileFrustumStepSize;
	m_tileStep            = tileStep;
	m_tileSize            = tileSize;
	m_tileCountX          = tileCountX;
	m_tileCountY          = tileCountY;
	m_resolutionX         = resolutionX;

	u32 tileCount = tileCountX * tileCountY;

	m_corners.resize(tileCount * 4);
	m_planes.resize(tileCount * 4);

	for (u32 y = 0; y < tileCountY; ++y)
	{
		for (u32 x = 0; x < tileCountX; ++x)
		{
			const Vec2 tileTopLeft = Vec2(float(x), float(y));
			u32        tileId      = x + y * tileCountX;
			computeTileFrustumParameters(
			    cameraFrustumTopLeftCorner, tileStep, tileTopLeft, &m_corners[tileId * 4], &m_planes[tileId * 4]);
		}
	}
}
