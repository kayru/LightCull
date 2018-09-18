#include "ClusteredLightBuilder.h"
#include "Utils.h"

#include <Rush/GfxDevice.h>
#include <Rush/UtilTimer.h>

#include <algorithm>

ClusteredLightBuilder::ClusteredLightBuilder(u32 maxLights)
: m_maxLights(maxLights), m_lightIntervals(maxLights), m_lightScreenSpaceExtents(maxLights)
{
	{
		GfxBufferDesc bufferDesc;
		bufferDesc.count  = 0;
		bufferDesc.stride = 4;
		bufferDesc.flags  = GfxBufferFlags::Transient | GfxBufferFlags::Storage;
		bufferDesc.format = GfxFormat_R16_Uint;
		m_lightIndexBuffer.takeover(Gfx_CreateBuffer(bufferDesc));
	}

	{
		GfxBufferDesc bufferDesc;
		bufferDesc.count  = 0;
		bufferDesc.stride = (u32)sizeof(LightGridCell);
		bufferDesc.flags  = GfxBufferFlags::Transient | GfxBufferFlags::Storage;
		bufferDesc.format = GfxFormat_Unknown;
		m_lightGridBuffer.takeover(Gfx_CreateBuffer(bufferDesc));
	}
}

ClusteredLightBuildResult ClusteredLightBuilder::build(GfxContext* ctx,
    const Camera&                                                  camera,
    const std::vector<LightSource>&                                viewSpaceLights,
    const BuildParams&                                             buildParams)
{
	m_lightDataSize = 0;
	m_lightGridSize = 0;

	ClusteredLightBuildResult result;
	Timer                     timer;

	const Vec2           resolutionF = Vec2((float)buildParams.resolution.x, (float)buildParams.resolution.y);
	const GfxCapability& caps        = Gfx_GetCapability();

	const Mat4 matProj = camera.buildProjMatrix(ProjectionFlags::Default);

	const float cameraNearZ = camera.getNearPlane();

	const Frustum frustum(matProj);
	const Vec3    cameraPosition = camera.getPosition();
	const Vec3    cameraForward  = camera.getForward();

	// assign lights to tiles

	u32 tileCountX    = divUp(buildParams.resolution.x, buildParams.tileSize);
	u32 tileCountY    = divUp(buildParams.resolution.y, buildParams.tileSize);
	u32 tilesPerSlice = tileCountX * tileCountY;
	u32 cellCount     = tilesPerSlice * buildParams.sliceCount;

	m_tileLightCount.clear();
	m_tileLightCount.resize(tilesPerSlice);

	m_lightGrid.resize(cellCount);

	memset(m_lightGrid.data(), 0, sizeof(LightGridCell) * cellCount);

	m_cellLightCount.clear();
	m_cellLightCount.resize(cellCount, 0);

	m_lightScreenSpaceExtents.m_count = viewSpaceLights.size();

	result.lightCullTime -= timer.time();
	performLightCullingAndComputeDepthIntervals(frustum, viewSpaceLights, m_visibleLightIndices, m_lightIntervals);
	result.lightCullTime += timer.time();

	result.visibleLightCount = (u32)m_lightIntervals.size();

	result.lightAssignTime -= timer.time();

	const DepthExtentsCalculator depthExtentsCalculator(buildParams);

	alignas(16) Mat4 matProjScreenSpace =
	    matProj * Mat4::scaleTranslate(Vec3(0.5f * resolutionF.x, -0.5f * resolutionF.y, 1.0f),
	                  Vec3(0.5f * resolutionF.x, 0.5f * resolutionF.y, 0.0f));

	performLightBinning(depthExtentsCalculator, matProjScreenSpace, cameraNearZ, buildParams.tileSize, tileCountX,
	    tileCountY, viewSpaceLights, m_lightIntervals, m_lightScreenSpaceExtents.data(), m_cellLightCount.data());

	for (size_t cellIndex = 0; cellIndex < m_cellLightCount.size(); ++cellIndex)
	{
		m_lightGrid[cellIndex].lightCount = m_cellLightCount[cellIndex];
	}

	if (buildParams.calculateTileLightCount)
	{
		for (size_t cellIndex = 0; cellIndex < m_lightGrid.size(); ++cellIndex)
		{
			const auto& cell = m_lightGrid[cellIndex];

			u32 cellX = (cellIndex % tilesPerSlice) % tileCountX;
			u32 cellY = (cellIndex % tilesPerSlice) / tileCountX;

			u32 tileIndex = cellX + cellY * tileCountX;

			m_tileLightCount[tileIndex] += m_lightGrid[cellIndex].lightCount;
		}
	}

	u32 assignedLightCount = 0;
	for (auto& cell : m_lightGrid)
	{
		cell.lightOffset = assignedLightCount;
		assignedLightCount += cell.lightCount;
		cell.lightCount = 0;
	}

	m_gpuLightIndices.resize(assignedLightCount);

	const float yHeight                    = tan(camera.getFov() / 2) * 2;
	const float xWidth                     = yHeight * camera.getAspect();
	const Vec2  cameraFrustumTopLeftCorner = Vec2(-xWidth / 2, yHeight / 2);
	const float tileFrustumStepSize        = xWidth * float(buildParams.tileSize) / float(buildParams.resolution.x);
	const Vec2  tileStep                   = Vec2(tileFrustumStepSize, -tileFrustumStepSize);

	m_tileFrustumCache.build(
	    camera.getFov(), camera.getAspect(), buildParams.tileSize, tileCountX, tileCountY, buildParams.resolution.x);

	parallelForEach(m_lightIntervals.begin(), m_lightIntervals.end(), [&](const LightDepthInterval& interval) {
		const LightTileDepthExtents&       depthExtents       = interval.depthExtents;
		const LightTileScreenSpaceExtents& screenSpaceExtents = m_lightScreenSpaceExtents[interval.lightIndex];

		const LightSource& light        = viewSpaceLights[interval.lightIndex];
		const Vec4         lightSphere  = Vec4(light.position, light.attenuationEnd);
		const Vec2 tileSpaceLightCenter = screenSpaceExtents.screenSpaceBox.center() / float(buildParams.tileSize);

		for (u32 y = screenSpaceExtents.tileMin.y; y <= screenSpaceExtents.tileMax.y; ++y)
		{
			for (u32 x = screenSpaceExtents.tileMin.x; x <= screenSpaceExtents.tileMax.x; ++x)
			{
				if (buildParams.useTileFrustumCulling)
				{
					u32         tileId  = x + y * tileCountX;
					const Vec3* corners = &m_tileFrustumCache.m_corners[tileId * 4];
					const Vec3* planes  = &m_tileFrustumCache.m_planes[tileId * 4];

					// const Vec2 tileTopLeft = Vec2(float(x), float(y));
					// TODO: implement culling on Z axis as well
					// if (!testTileFrustumSphereFastButInaccurate(cameraFrustumTopLeftCorner, tileStep, tileTopLeft,
					// tileSpaceLightCenter, lightSphere))
					if (!testTileFrustumSphere(corners, planes, lightSphere))
					{
						continue;
					}
				}

				for (u32 z = depthExtents.sliceMin; z <= depthExtents.sliceMax; ++z)
				{
					u32            cellIndex = x + y * tileCountX + z * tilesPerSlice;
					LightGridCell& cell      = m_lightGrid[cellIndex];

					u32 writeIndex = interlockedIncrement(cell.lightCount) - 1;
					u32 offset     = cell.lightOffset + writeIndex;

					m_gpuLightIndices[offset] = interval.lightIndex;
				}
			}
		}
	});

	result.lightAssignTime += timer.time();

	result.uploadTime -= timer.time();
	m_lightGridSize = Gfx_UpdateBufferT(ctx, m_lightGridBuffer, m_lightGrid);
	m_lightDataSize += Gfx_UpdateBufferT(ctx, m_lightIndexBuffer, m_gpuLightIndices);

	result.uploadTime += timer.time();
	result.totalDataSize = m_lightDataSize + m_lightGridSize;

	result.buildTotalTime = timer.time();

	result.lightGridBuffer  = m_lightGridBuffer.get();
	result.lightIndexBuffer = m_lightIndexBuffer.get();

	return result;
}
