#include "TiledLightTreeBuilder.h"
#include "Utils.h"

#include <Rush/UtilTimer.h>

#include <algorithm>

// Auto-generated using GenerateTraversalLUT.py
// clang-format off
static const LightTreeLutItem g_lightTreeLut[] =
{
	{ 0x00,0x80000001 },{ 0x02,0x00000003 },{ 0x00,0x80000001 },{ 0x01,0x80000001 },
	{ 0x06,0x00000007 },{ 0x04,0x00000003 },{ 0x00,0x80000001 },{ 0x01,0x80000001 },
	{ 0x05,0x00000003 },{ 0x02,0x80000001 },{ 0x03,0x80000001 },{ 0x0e,0x0000000f },
	{ 0x0c,0x00000007 },{ 0x08,0x00000003 },{ 0x00,0x80000001 },{ 0x01,0x80000001 },
	{ 0x09,0x00000003 },{ 0x02,0x80000001 },{ 0x03,0x80000001 },{ 0x0d,0x00000007 },
	{ 0x0a,0x00000003 },{ 0x04,0x80000001 },{ 0x05,0x80000001 },{ 0x0b,0x00000003 },
	{ 0x06,0x80000001 },{ 0x07,0x80000001 },{ 0x1e,0x0000001f },{ 0x1c,0x0000000f },
	{ 0x18,0x00000007 },{ 0x10,0x00000003 },{ 0x00,0x80000001 },{ 0x01,0x80000001 },
	{ 0x11,0x00000003 },{ 0x02,0x80000001 },{ 0x03,0x80000001 },{ 0x19,0x00000007 },
	{ 0x12,0x00000003 },{ 0x04,0x80000001 },{ 0x05,0x80000001 },{ 0x13,0x00000003 },
	{ 0x06,0x80000001 },{ 0x07,0x80000001 },{ 0x1d,0x0000000f },{ 0x1a,0x00000007 },
	{ 0x14,0x00000003 },{ 0x08,0x80000001 },{ 0x09,0x80000001 },{ 0x15,0x00000003 },
	{ 0x0a,0x80000001 },{ 0x0b,0x80000001 },{ 0x1b,0x00000007 },{ 0x16,0x00000003 },
	{ 0x0c,0x80000001 },{ 0x0d,0x80000001 },{ 0x17,0x00000003 },{ 0x0e,0x80000001 },
	{ 0x0f,0x80000001 },{ 0x3e,0x0000003f },{ 0x3c,0x0000001f },{ 0x38,0x0000000f },
	{ 0x30,0x00000007 },{ 0x20,0x00000003 },{ 0x00,0x80000001 },{ 0x01,0x80000001 },
	{ 0x21,0x00000003 },{ 0x02,0x80000001 },{ 0x03,0x80000001 },{ 0x31,0x00000007 },
	{ 0x22,0x00000003 },{ 0x04,0x80000001 },{ 0x05,0x80000001 },{ 0x23,0x00000003 },
	{ 0x06,0x80000001 },{ 0x07,0x80000001 },{ 0x39,0x0000000f },{ 0x32,0x00000007 },
	{ 0x24,0x00000003 },{ 0x08,0x80000001 },{ 0x09,0x80000001 },{ 0x25,0x00000003 },
	{ 0x0a,0x80000001 },{ 0x0b,0x80000001 },{ 0x33,0x00000007 },{ 0x26,0x00000003 },
	{ 0x0c,0x80000001 },{ 0x0d,0x80000001 },{ 0x27,0x00000003 },{ 0x0e,0x80000001 },
	{ 0x0f,0x80000001 },{ 0x3d,0x0000001f },{ 0x3a,0x0000000f },{ 0x34,0x00000007 },
	{ 0x28,0x00000003 },{ 0x10,0x80000001 },{ 0x11,0x80000001 },{ 0x29,0x00000003 },
	{ 0x12,0x80000001 },{ 0x13,0x80000001 },{ 0x35,0x00000007 },{ 0x2a,0x00000003 },
	{ 0x14,0x80000001 },{ 0x15,0x80000001 },{ 0x2b,0x00000003 },{ 0x16,0x80000001 },
	{ 0x17,0x80000001 },{ 0x3b,0x0000000f },{ 0x36,0x00000007 },{ 0x2c,0x00000003 },
	{ 0x18,0x80000001 },{ 0x19,0x80000001 },{ 0x2d,0x00000003 },{ 0x1a,0x80000001 },
	{ 0x1b,0x80000001 },{ 0x37,0x00000007 },{ 0x2e,0x00000003 },{ 0x1c,0x80000001 },
	{ 0x1d,0x80000001 },{ 0x2f,0x00000003 },{ 0x1e,0x80000001 },{ 0x1f,0x80000001 },
	{ 0x7e,0x0000007f },{ 0x7c,0x0000003f },{ 0x78,0x0000001f },{ 0x70,0x0000000f },
	{ 0x60,0x00000007 },{ 0x40,0x00000003 },{ 0x00,0x80000001 },{ 0x01,0x80000001 },
	{ 0x41,0x00000003 },{ 0x02,0x80000001 },{ 0x03,0x80000001 },{ 0x61,0x00000007 },
	{ 0x42,0x00000003 },{ 0x04,0x80000001 },{ 0x05,0x80000001 },{ 0x43,0x00000003 },
	{ 0x06,0x80000001 },{ 0x07,0x80000001 },{ 0x71,0x0000000f },{ 0x62,0x00000007 },
	{ 0x44,0x00000003 },{ 0x08,0x80000001 },{ 0x09,0x80000001 },{ 0x45,0x00000003 },
	{ 0x0a,0x80000001 },{ 0x0b,0x80000001 },{ 0x63,0x00000007 },{ 0x46,0x00000003 },
	{ 0x0c,0x80000001 },{ 0x0d,0x80000001 },{ 0x47,0x00000003 },{ 0x0e,0x80000001 },
	{ 0x0f,0x80000001 },{ 0x79,0x0000001f },{ 0x72,0x0000000f },{ 0x64,0x00000007 },
	{ 0x48,0x00000003 },{ 0x10,0x80000001 },{ 0x11,0x80000001 },{ 0x49,0x00000003 },
	{ 0x12,0x80000001 },{ 0x13,0x80000001 },{ 0x65,0x00000007 },{ 0x4a,0x00000003 },
	{ 0x14,0x80000001 },{ 0x15,0x80000001 },{ 0x4b,0x00000003 },{ 0x16,0x80000001 },
	{ 0x17,0x80000001 },{ 0x73,0x0000000f },{ 0x66,0x00000007 },{ 0x4c,0x00000003 },
	{ 0x18,0x80000001 },{ 0x19,0x80000001 },{ 0x4d,0x00000003 },{ 0x1a,0x80000001 },
	{ 0x1b,0x80000001 },{ 0x67,0x00000007 },{ 0x4e,0x00000003 },{ 0x1c,0x80000001 },
	{ 0x1d,0x80000001 },{ 0x4f,0x00000003 },{ 0x1e,0x80000001 },{ 0x1f,0x80000001 },
	{ 0x7d,0x0000003f },{ 0x7a,0x0000001f },{ 0x74,0x0000000f },{ 0x68,0x00000007 },
	{ 0x50,0x00000003 },{ 0x20,0x80000001 },{ 0x21,0x80000001 },{ 0x51,0x00000003 },
	{ 0x22,0x80000001 },{ 0x23,0x80000001 },{ 0x69,0x00000007 },{ 0x52,0x00000003 },
	{ 0x24,0x80000001 },{ 0x25,0x80000001 },{ 0x53,0x00000003 },{ 0x26,0x80000001 },
	{ 0x27,0x80000001 },{ 0x75,0x0000000f },{ 0x6a,0x00000007 },{ 0x54,0x00000003 },
	{ 0x28,0x80000001 },{ 0x29,0x80000001 },{ 0x55,0x00000003 },{ 0x2a,0x80000001 },
	{ 0x2b,0x80000001 },{ 0x6b,0x00000007 },{ 0x56,0x00000003 },{ 0x2c,0x80000001 },
	{ 0x2d,0x80000001 },{ 0x57,0x00000003 },{ 0x2e,0x80000001 },{ 0x2f,0x80000001 },
	{ 0x7b,0x0000001f },{ 0x76,0x0000000f },{ 0x6c,0x00000007 },{ 0x58,0x00000003 },
	{ 0x30,0x80000001 },{ 0x31,0x80000001 },{ 0x59,0x00000003 },{ 0x32,0x80000001 },
	{ 0x33,0x80000001 },{ 0x6d,0x00000007 },{ 0x5a,0x00000003 },{ 0x34,0x80000001 },
	{ 0x35,0x80000001 },{ 0x5b,0x00000003 },{ 0x36,0x80000001 },{ 0x37,0x80000001 },
	{ 0x77,0x0000000f },{ 0x6e,0x00000007 },{ 0x5c,0x00000003 },{ 0x38,0x80000001 },
	{ 0x39,0x80000001 },{ 0x5d,0x00000003 },{ 0x3a,0x80000001 },{ 0x3b,0x80000001 },
	{ 0x6f,0x00000007 },{ 0x5e,0x00000003 },{ 0x3c,0x80000001 },{ 0x3d,0x80000001 },
	{ 0x5f,0x00000003 },{ 0x3e,0x80000001 },{ 0x3f,0x80000001 }
};
// clang-format on

const LightTreeLutItem* getLightTreeLut(u32 index)
{
	RUSH_ASSERT(index < TiledLightTreeBuilder::MaxBottomUpTreeLevels)
	static const int lutOffsets[] = {0, 1, 4, 11, 26, 57, 120};
	return &g_lightTreeLut[lutOffsets[index]];
}

TiledLightTreeBuilder::TiledLightTreeBuilder(u32 maxLights)
: m_maxLights(maxLights), m_lightIntervals(maxLights), m_lightScreenSpaceExtents(maxLights)
{
	{
		GfxBufferDesc bufferDesc;
		bufferDesc.count  = 0;
		bufferDesc.stride = 4;
		bufferDesc.mode   = GfxBufferMode::Temporary;
		bufferDesc.format = GfxFormat_R16_Uint;
		bufferDesc.type   = GfxBufferType::Storage;
		m_lightIndexBuffer.takeover(Gfx_CreateBuffer(bufferDesc));
	}

	{
		GfxBufferDesc bufferDesc;
		bufferDesc.count  = m_maxLights;
		bufferDesc.stride = (u32)sizeof(PackedLightTreeNode);
		bufferDesc.mode   = GfxBufferMode::Temporary;
		bufferDesc.format = GfxFormat_Unknown;
		bufferDesc.type   = GfxBufferType::Storage;
		m_lightTreeBuffer.takeover(Gfx_CreateBuffer(bufferDesc));
	}

	{
		GfxBufferDesc bufferDesc;
		bufferDesc.count  = 0;
		bufferDesc.stride = (u32)sizeof(LightGridCell);
		bufferDesc.mode   = GfxBufferMode::Temporary;
		bufferDesc.format = GfxFormat_Unknown;
		bufferDesc.type   = GfxBufferType::Storage;
		m_lightTileInfoBuffer.takeover(Gfx_CreateBuffer(bufferDesc));
	}

	{
		GfxBufferDesc bufferDesc;
		bufferDesc.count  = 0;
		bufferDesc.stride = (u32)sizeof(LightDepthInterval);
		bufferDesc.mode   = GfxBufferMode::Temporary;
		bufferDesc.format = GfxFormat_Unknown;
		bufferDesc.type   = GfxBufferType::Storage;
		m_lightDepthIntervalBuffer.takeover(Gfx_CreateBuffer(bufferDesc));
	}

	{
		GfxBufferDesc bufferDesc;
		bufferDesc.count  = 0;
		bufferDesc.stride = (u32)sizeof(u32);
		bufferDesc.mode   = GfxBufferMode::Temporary;
		bufferDesc.format = GfxFormat_Unknown;
		bufferDesc.type   = GfxBufferType::Storage;
		m_lightDepthIntervalIndexBuffer.takeover(Gfx_CreateBuffer(bufferDesc));
	}
}

struct DepthInterval
{
	float min;
	float max;
	float diameter;
};

static DepthInterval findDepthInterval(
    const LightDepthInterval* intervals, const u16* intervalIndices, u32 first, u32 last)
{
	DepthInterval result;
	result.min = FLT_MAX;
	result.max = -FLT_MAX;
	for (u32 i = first; i < last; ++i)
	{
		const LightDepthInterval& interval = intervals[intervalIndices[i]];
		result.min                         = min(result.min, interval.center - interval.radius);
		result.max                         = max(result.max, interval.center + interval.radius);
	}
	result.diameter = result.max - result.min;
	return result;
}

inline PackedLightTreeNode packNode(const LightTreeNode& node, u32 nodeIndex)
{
	PackedLightTreeNode result;

	result.center      = (node.depthMin + node.depthMax) / 2.0f;
	result.radius      = (node.depthMax - node.depthMin) / 2.0f;
	result.lightOffset = node.lightOffset;

	u32 skipCount = node.next - nodeIndex;
	result.params = packNodeParams(node.lightCount, node.isLeaf, skipCount);

	return result;
}

struct LightTreeInfo
{
	u32 totalNodeCount;
	u32 leafNodeCount;
};

inline LightTreeInfo buildLightTreeInfo(const TreeBuildParams& params, u32 lightCount)
{
	LightTreeInfo result;

	const u32 targetLeafNodeCount = divUp(lightCount, params.minLightsPerLeaf);

	// Number of leaf nodes is always a power of 2.

#if 0
	// round-down to pow2
	const u32 leafNodeCount = min<u32>(1 << (31 - bitScanReverse(targetLeafNodeCount)), params.maxLeafNodes);
#else
	// round up to pow2
	const u32 leafNodeCount = min<u32>(nextPow2(targetLeafNodeCount), params.maxLeafNodes);
#endif

	result.leafNodeCount  = leafNodeCount;
	result.totalNodeCount = leafNodeCount * 2 - 1;

	return result;
}

static u32 buildLightTreeBottomUp(const TreeBuildParams& params, const AlignedArray<LightDepthInterval>& intervals,
    const AlignedArray<u16>& intervalIndices, u32 lightOffset, u32 lightCount, PackedLightTreeNode* outDepthFirstTree,
    u32 debugIndex)
{
	const LightTreeInfo treeInfo = buildLightTreeInfo(params, lightCount);

	alignas(32) LightTreeNode defaultNode;
	defaultNode.lightOffset = 0;
	defaultNode.lightCount  = 0;
	defaultNode.left        = ~0u;
	defaultNode.right       = ~0u;
	defaultNode.isLeaf      = 1;
	defaultNode.next        = ~0u;
	defaultNode.depthMin    = FLT_MAX / 2.0f;
	defaultNode.depthMax    = -FLT_MAX / 2.0f;

	alignas(32) LightTreeNode breadthFirstTree[TiledLightTreeBuilder::MaxTotalNodes];

	// Assign lights to leaf nodes

	const u32 lightsPerNode     = divUp(lightCount, treeInfo.leafNodeCount);
	const u32 usedLeafNodeCount = divUp(lightCount, lightsPerNode);

	u32 assignedLights = 0;

	for (u32 leafNodeIndex = 0; leafNodeIndex < usedLeafNodeCount; ++leafNodeIndex)
	{
		const u32 nodeLightCount = min(lightsPerNode, lightCount - assignedLights);

		LightTreeNode& leafNode = breadthFirstTree[leafNodeIndex];

		leafNode             = defaultNode;
		leafNode.lightOffset = lightOffset + leafNodeIndex * lightsPerNode;
		leafNode.lightCount  = nodeLightCount;

		for (u32 i = 0; i < nodeLightCount; ++i)
		{
			const u32                 lightIndex = leafNode.lightOffset + i;
			const LightDepthInterval& interval   = intervals[intervalIndices[lightIndex]];
			leafNode.depthMin                    = min(leafNode.depthMin, interval.center - interval.radius);
			leafNode.depthMax                    = max(leafNode.depthMax, interval.center + interval.radius);
		}

		assignedLights += nodeLightCount;
	}

	// Ensure that all leaf level nodes are initialized

	for (u32 leafNodeIndex = usedLeafNodeCount; leafNodeIndex < treeInfo.leafNodeCount; ++leafNodeIndex)
	{
		LightTreeNode& leafNode = breadthFirstTree[leafNodeIndex];
		leafNode                = breadthFirstTree[usedLeafNodeCount - 1];
		leafNode.lightCount     = 0;
		leafNode.depthMax       = leafNode.depthMin;
	}

	RUSH_ASSERT(assignedLights == lightCount);

	// Go through upper levels of the tree and compute their metadata

	u32            currentNodeIndex = treeInfo.leafNodeCount; // first node of the non-leaf level
	LightTreeNode* currentNode      = breadthFirstTree + currentNodeIndex;

	while (currentNodeIndex < treeInfo.totalNodeCount)
	{
		u32 leftIndex  = (currentNodeIndex << 1) & treeInfo.totalNodeCount;
		u32 rightIndex = leftIndex + 1;

		const LightTreeNode& leftNode  = breadthFirstTree[leftIndex];
		const LightTreeNode& rightNode = breadthFirstTree[rightIndex];

		currentNode->depthMin = min(leftNode.depthMin, rightNode.depthMin);
		currentNode->depthMax = max(leftNode.depthMax, rightNode.depthMax);

		RUSH_ASSERT(leftNode.lightOffset <= rightNode.lightOffset);

		currentNode->lightOffset = leftNode.lightOffset;
		currentNode->lightCount  = leftNode.lightCount + rightNode.lightCount;

		currentNode->left  = leftIndex;
		currentNode->right = rightIndex;

		currentNode->isLeaf = 0;

		currentNodeIndex++;
		currentNode++;
	}

	RUSH_ASSERT(currentNodeIndex == treeInfo.totalNodeCount);

	// Convert breadth-first tree into depth-first with skip pointers using precomputed LUT

	u32                     lutIndex = 31 - bitScanReverse(treeInfo.totalNodeCount);
	const LightTreeLutItem* lut      = getLightTreeLut(lutIndex);

	for (u32 nodeIndex = 0; nodeIndex < treeInfo.totalNodeCount; ++nodeIndex)
	{
		const LightTreeLutItem& templateNode = lut[nodeIndex];
		const LightTreeNode&    bfNode       = breadthFirstTree[templateNode.breadthFirstIndex];

		PackedLightTreeNode& dfNode = outDepthFirstTree[nodeIndex];
		dfNode.center               = (bfNode.depthMin + bfNode.depthMax) / 2.0f;
		dfNode.radius               = (bfNode.depthMax - bfNode.depthMin) / 2.0f;
		dfNode.lightOffset          = bfNode.lightOffset;
		dfNode.params               = packNodeParams(bfNode.lightCount, 0, 0) | templateNode.params;
	}

	return treeInfo.totalNodeCount;
}

TiledLightTreeBuildResult TiledLightTreeBuilder::build(GfxContext* ctx,
    const Camera&                                                  camera,
    const std::vector<LightSource>&                                viewSpaceLights,
    const TiledLightTreeBuildParams&                               buildParams)
{
	RUSH_ASSERT(buildParams.maxLeafNodes <= MaxLeafNodes);

	TiledLightTreeBuildResult result;
	Timer                     timer;

	const auto& resolution = buildParams.resolution;
	const u32   tileSize   = buildParams.tileSize;

	const Vec2           resolutionF = Vec2((float)resolution.x, (float)resolution.y);
	const GfxCapability& caps        = Gfx_GetCapability();

	const Mat4 matProj = camera.buildProjMatrix(ProjectionFlags::Default);

	const float cameraNearZ = camera.getNearPlane();

	const u32 regularSliceCount =
	    buildParams.sliceCount - 1; // N linearly spaced slices, followed by light interval hierarchy
	const float maxSliceDepth = buildParams.maxSliceDepth;
	const u32   sliceCount    = buildParams.sliceCount;

	const Frustum frustum(matProj);
	const Vec3    cameraPosition = camera.getPosition();
	const Vec3    cameraForward  = camera.getForward();

	m_lightIntervals.clear();
	m_lightScreenSpaceExtents.m_count = viewSpaceLights.size();

	result.lightCullTime -= timer.time();
	performLightCullingAndComputeDepthIntervals(frustum, viewSpaceLights, m_visibleLightIndices, m_lightIntervals);
	result.lightCullTime += timer.time();

	// light intervals only need to be sorted once
	// trees for all tiles can be built assuming pre-sorted data

	result.visibleLightCount = (u32)m_lightIntervals.size();

	// assign lights to tiles

	result.lightAssignTime -= timer.time();

	const u32 tileCountX     = divUp(resolution.x, tileSize);
	const u32 tileCountY     = divUp(resolution.y, tileSize);
	const u32 tilesPerSlice  = tileCountX * tileCountY;
	const u32 totalCellCount = tilesPerSlice * sliceCount;

	const DepthExtentsCalculator depthExtentsCalculator(buildParams);

	if (m_lightGrid.m_capacity < totalCellCount)
	{
		m_lightGrid = AlignedArray<LightGridCell>(totalCellCount);
	}

	m_lightGrid.m_count = totalCellCount;
	memset(m_lightGrid.data(), 0, sizeof(LightGridCell) * totalCellCount);

	m_cellLightCount.clear();
	m_cellLightCount.resize(totalCellCount, 0);

	alignas(16) Mat4 matProjScreenSpace =
	    matProj * Mat4::scaleTranslate(Vec3(0.5f * resolutionF.x, -0.5f * resolutionF.y, 1.0f),
	                  Vec3(0.5f * resolutionF.x, 0.5f * resolutionF.y, 0.0f));

	const u32 totalBinnedLightCount =
	    performLightBinning(depthExtentsCalculator, matProjScreenSpace, cameraNearZ, buildParams.tileSize, tileCountX,
	        tileCountY, viewSpaceLights, m_lightIntervals, m_lightScreenSpaceExtents.data(), m_cellLightCount.data());

	m_tileLightCount.clear();
	m_tileLightCount.resize(tilesPerSlice);

	if (buildParams.calculateTileLightCount)
	{
		for (size_t cellIndex = 0; cellIndex < m_lightGrid.size(); ++cellIndex)
		{
			u32 cellX = (cellIndex % tilesPerSlice) % tileCountX;
			u32 cellY = (cellIndex % tilesPerSlice) / tileCountX;

			u32 tileIndex = cellX + cellY * tileCountX;

			m_tileLightCount[tileIndex] += m_cellLightCount[cellIndex];
		}
	}

	m_tileIntervalIndices.resize(totalBinnedLightCount);
	m_tileIntervalIndicesSorted.resize(totalBinnedLightCount);

	u32 assignedLightCount = 0;
	for (size_t cellIndex = 0; cellIndex < totalCellCount; ++cellIndex)
	{
		LightGridCell& cell        = m_lightGrid[cellIndex];
		cell.lightOffset           = assignedLightCount;
		u32 conservativeLightCount = m_cellLightCount[cellIndex];
		assignedLightCount += m_cellLightCount[cellIndex];

		cell.lightCount = 0; // to be filled when we scatter lights
	}

	// copy light intervals into tiles (making this parallel is not a win due to interlocked ops)
	// additionally, the order of intervals must be preserved

	const float yHeight                    = tan(camera.getFov() / 2) * 2;
	const float xWidth                     = yHeight * camera.getAspect();
	const Vec2  cameraFrustumTopLeftCorner = Vec2(-xWidth / 2, yHeight / 2);
	const float tileFrustumStepSize        = xWidth * float(buildParams.tileSize) / float(buildParams.resolution.x);
	const Vec2  tileStep                   = Vec2(tileFrustumStepSize, -tileFrustumStepSize);

	m_tileFrustumCache.build(
	    camera.getFov(), camera.getAspect(), buildParams.tileSize, tileCountX, tileCountY, buildParams.resolution.x);

	parallelForEach(m_lightIntervals.begin(), m_lightIntervals.end(), [&](const LightDepthInterval& interval) {
		const u64 intervalIndex = &interval - m_lightIntervals.data();

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

					if (!testTileFrustumSphere(corners, planes, lightSphere))
					{
						continue;
					}
				}

				for (u32 z = depthExtents.sliceMin; z <= depthExtents.sliceMax; ++z)
				{
					u32   cellIndex  = x + y * tileCountX + z * tilesPerSlice;
					auto& cell       = m_lightGrid[cellIndex];
					u16   writeIndex = interlockedIncrement(cell.lightCount) - 1;
					u16*  writePtr   = &m_tileIntervalIndices[cell.lightOffset] + writeIndex;
					*writePtr        = (u16)intervalIndex;
				}
			}
		}
	});

	result.lightAssignTime += timer.time();

	result.buildTreeTime -= timer.time();

	// build the per-tile trees

	m_gpuLights.clear();
	m_gpuLightTree.clear();
	m_gpuLightIndices.clear();

	m_gpuLightIndices.resize(m_tileIntervalIndices.size());
	m_gpuLightTree.reserve((m_lightIntervals.size() * 4) / 3);

	auto computeSliceDepth = [&](u32 slice) {
		if (slice == 0)
			return 0.0f;
		if (slice >= buildParams.sliceCount)
			return 100000.0f;
		return buildParams.useExponentialSlices
		           ? computeExponentialSliceDepth(slice, buildParams.maxSliceDepth, buildParams.sliceCount)
		           : computeLinearSliceDepth(slice, buildParams.maxSliceDepth, buildParams.sliceCount);
	};

	m_treeBuildQueue.clear();
	m_treeBuildQueue.reserve(totalCellCount);

	TreeBuildParams treeBuildParams;
	treeBuildParams.minLightsPerLeaf = buildParams.targetLightsPerLeaf;
	treeBuildParams.maxLeafNodes     = buildParams.maxLeafNodes;

	u32 copiedGpuLightCount = 0;

	for (u32 z = 0; z < sliceCount; ++z)
	{
		const float sliceDepthMin    = computeSliceDepth(z);
		const float sliceDepthMax    = computeSliceDepth(z + 1);
		const float sliceDepthExtent = sliceDepthMax - sliceDepthMin;

		const float treeHeuristicExtentsThreshold =
		    buildParams.useClippedLightExtents ? sliceDepthExtent * buildParams.lightTreeHeuristic * 0.5f // use radius
		                                       : sliceDepthExtent * buildParams.lightTreeHeuristic; // use diameter

		for (u32 y = 0; y < tileCountY; ++y)
		{
			for (u32 x = 0; x < tileCountX; ++x)
			{
				u32            cellIndex = x + y * tileCountX + z * tilesPerSlice;
				LightGridCell& cell      = m_lightGrid[cellIndex];

				const u32 cellLightCount = cell.lightCount;

				if (cellLightCount == 0)
				{
					continue;
				}

				bool useLightList = true;

				if (cellLightCount > (u32)buildParams.targetLightsPerLeaf)
				{
					float lightExtentsSum = 0.0f;
					for (u32 lightIt = 0; lightIt < cellLightCount; ++lightIt)
					{
						u32                       tileIntervalIndex   = lightIt + cell.lightOffset;
						u32                       globalIntervalIndex = m_tileIntervalIndices[tileIntervalIndex];
						const LightDepthInterval& interval            = m_lightIntervals[globalIntervalIndex];

						if (buildParams.useClippedLightExtents)
						{
							float lightDepthMin = max(interval.center - interval.radius, sliceDepthMin);
							float lightDepthMax = min(interval.center + interval.radius, sliceDepthMax);
							lightExtentsSum += lightDepthMax - lightDepthMin;
						}
						else
						{
							lightExtentsSum += interval.radius * 2;
						}
					}

					float averageLightExtents = lightExtentsSum / cellLightCount;

					if (averageLightExtents <= treeHeuristicExtentsThreshold)
					{
						useLightList = false;
					}
				}

				if (useLightList)
				{
					LightTreeNode node;
					node.depthMin    = -FLT_MAX / 2.0f;
					node.depthMax    = FLT_MAX / 2.0f;
					node.lightOffset = cell.lightOffset;
					node.lightCount  = cellLightCount;
					node.left        = ~0u;
					node.right       = ~0u;
					node.isLeaf      = 1;
					node.next        = ~0u;

					cell.treeNodeCount = 1;
					cell.treeOffset    = (u32)m_gpuLightTree.size();

					m_gpuLightTree.push_back(packNode(node, 0));

					for (u32 i = 0; i < cellLightCount; ++i)
					{
						u32                       tileIntervalIndex   = i + cell.lightOffset;
						u32                       globalIntervalIndex = m_tileIntervalIndices[tileIntervalIndex];
						const LightDepthInterval& interval            = m_lightIntervals[globalIntervalIndex];

						m_gpuLightIndices[cell.lightOffset + i] = interval.lightIndex;
					}

					result.listCellCount++;
				}
				else
				{
					LightTreeInfo buildInfo = buildLightTreeInfo(treeBuildParams, cellLightCount);

					cell.treeNodeCount = buildInfo.totalNodeCount;
					cell.treeOffset    = (u32)m_gpuLightTree.size();

					m_gpuLightTree.resize(m_gpuLightTree.size() + buildInfo.totalNodeCount);
					m_treeBuildQueue.push_back(cellIndex);

					result.treeCellCount++;
				}

				copiedGpuLightCount += cellLightCount;
			}
		}
	}

	result.buildTreeTime += timer.time();

	// Build per-cell trees

	{
		result.buildTreeTime -= timer.time();

		parallelForEach(m_treeBuildQueue.begin(), m_treeBuildQueue.end(), [&](u32 cellIndex) {
			const LightGridCell& cell = m_lightGrid[cellIndex];

			u16* idxBegin = &m_tileIntervalIndices[cell.lightOffset];
			u16* idxEnd   = idxBegin + cell.lightCount;

#if 0
			// Perfect sorting using std::sort is pretty slow and is not actually required to get good GPU performance.
			// An approximate radix sort / bucket sort is much faster on CPU and does not noticeably affect GPU.
			std::sort(idxBegin, idxEnd,
			    [&](u16 a, u16 b) { return m_lightIntervals[a].center < m_lightIntervals[b].center; });

			for (u32 i = cell.lightOffset; i < cell.lightOffset + cell.lightCount; ++i)
			{
				m_tileIntervalIndicesSorted[i] = m_tileIntervalIndices[i];
			}
#else
			// TODO: move the sorting into a function
			static constexpr u32 BUCKET_COUNT = 512;
			u16 buckets[BUCKET_COUNT];
			memset(buckets, 0, sizeof(buckets));
			float lightDepthMax = 0.0f;

			// TODO: compute max depth during binning
			for (u32 i = 0; i < cell.lightCount; ++i)
			{
				u16 lightIndex = idxBegin[i];
				lightDepthMax = max(lightDepthMax, m_lightIntervals[lightIndex].center);
			}

			// Compute histogram
			float depthScale = float(BUCKET_COUNT) / lightDepthMax;

			auto computeLightBucket = [depthScale](float depth)
			{
				return clamp<int>(int(depthScale * depth), 0, int(BUCKET_COUNT-1));
			};

			for (u32 i = 0; i < cell.lightCount; ++i)
			{
				u16 lightIndex = idxBegin[i];
				int bucketIndex = computeLightBucket(m_lightIntervals[lightIndex].center);
				buckets[bucketIndex]++;
			}

			// Perform prefix scan
			u16 offset = 0;
			for (u32 i = 0; i < BUCKET_COUNT; ++i)
			{
				u16 temp = buckets[i];
				buckets[i] = offset;
				offset += temp;
			}

			// Write out sorted light indices
			for (u32 i = 0; i < cell.lightCount; ++i)
			{
				u16 lightIndex = idxBegin[i];
				int bucketIndex = computeLightBucket(m_lightIntervals[lightIndex].center);
				u32 writeIndex = cell.lightOffset + buckets[bucketIndex];
				buckets[bucketIndex]++;
				m_tileIntervalIndicesSorted[writeIndex] = lightIndex;
			}
#endif

			buildLightTreeBottomUp(treeBuildParams, m_lightIntervals, m_tileIntervalIndicesSorted, cell.lightOffset,
			    cell.lightCount, &m_gpuLightTree[cell.treeOffset], cellIndex);

			for (u32 i = 0; i < cell.lightCount; ++i)
			{
				u32                       tileIntervalIndex   = i + cell.lightOffset;
				u32                       globalIntervalIndex = m_tileIntervalIndicesSorted[tileIntervalIndex];
				const LightDepthInterval& interval            = m_lightIntervals[globalIntervalIndex];

				m_gpuLightIndices[cell.lightOffset + i] = interval.lightIndex;
			}
		});

		RUSH_ASSERT(m_gpuLightIndices.size() == m_tileIntervalIndicesSorted.size());

		if (m_gpuLightTree.empty())
		{
			PackedLightTreeNode dummyNode = {};
			m_gpuLightTree.push_back(dummyNode);
		}

		{
			result.uploadTime -= timer.time();
			result.treeDataSize += Gfx_UpdateBufferT(ctx, m_lightTreeBuffer, m_gpuLightTree);
			result.uploadTime += timer.time();
		}

		result.buildTreeTime += timer.time();
	}

	// Upload light sources, light indices and tile info
	{
		result.uploadTime -= timer.time();

		result.treeDataSize = Gfx_UpdateBufferT(ctx, m_lightTileInfoBuffer, m_lightGrid);

		if (m_gpuLightIndices.empty())
		{
			m_gpuLightIndices.push_back(0);
		}

		{
			static_assert(sizeof(*viewSpaceLights.data()) == sizeof(*m_gpuLights.data()), "");
			result.lightDataSize += Gfx_UpdateBufferT(ctx, m_lightIndexBuffer, m_gpuLightIndices);
		}

		result.uploadTime += timer.time();
	}

	result.lightTreeBuffer     = m_lightTreeBuffer.get();
	result.lightIndexBuffer    = m_lightIndexBuffer.get();
	result.lightTileInfoBuffer = m_lightTileInfoBuffer.get();

	result.hierarchicalCullingDepthThreshold = maxSliceDepth;
	result.sliceCount                        = sliceCount;

	result.totalDataSize = result.treeDataSize + result.lightDataSize;

	result.buildTotalTime = timer.time();

	return result;
}
