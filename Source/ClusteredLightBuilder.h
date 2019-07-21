#pragma once

#include "LightingCommon.h"

#include <Rush/GfxCommon.h>
#include <Rush/MathTypes.h>
#include <Rush/Rush.h>
#include <Rush/UtilCamera.h>

struct ClusteredLightBuildResult
{
	double buildTotalTime;
	double lightCullTime   = 0;
	double lightAssignTime = 0;
	double uploadTime      = 0;

	double lightExtentsTime = 0;

	u32 totalDataSize     = 0;
	u32 visibleLightCount = 0;

	GfxBuffer lightGridBuffer;
	GfxBuffer lightIndexBuffer;
};

class ClusteredLightBuilder
{
public:
	ClusteredLightBuilder(u32 maxLights);

	struct LightGridCell
	{
		u32 lightOffset;
		u32 lightCount;
		u32 pad0;
		u32 pad1;
	};

	struct BuildParams : CommonLightBuildParams
	{
	};

	ClusteredLightBuildResult build(
	    GfxContext* ctx, const Camera& camera, const std::vector<LightSource>& lights, const BuildParams& params);

	const u32 m_maxLights;

	std::vector<u16>                          m_cellLightCount;
	std::vector<u16>                          m_tileLightCount;
	AlignedArray<u32>                         m_visibleLightIndices;
	AlignedArray<LightDepthInterval>          m_lightIntervals;
	AlignedArray<LightTileScreenSpaceExtents> m_lightScreenSpaceExtents;
	AlignedArray<LightGridCell>               m_lightGrid;
	std::vector<u16>                          m_gpuLightIndices;

	u32 m_lightDataSize = 0;
	u32 m_lightGridSize = 0;

	GfxOwn<GfxBuffer> m_lightGridBuffer;
	GfxOwn<GfxBuffer> m_lightIndexBuffer;

	TileFrustumCache m_tileFrustumCache;
};
