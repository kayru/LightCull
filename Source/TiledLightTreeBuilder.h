#pragma once

#include "GpuBuffer.h"
#include "LightingCommon.h"
#include "Shader.h"

#include <Rush/GfxCommon.h>
#include <Rush/GfxRef.h>
#include <Rush/MathTypes.h>
#include <Rush/UtilCamera.h>
#include <Rush/UtilTuple.h>

#include <vector>

struct LightTreeNode
{
	u32   lightOffset;
	u32   lightCount;
	u32   left;
	u32   right;
	u32   isLeaf;
	u32   next;
	float depthMin;
	float depthMax;
};

struct LightTreeLutItem
{
	u32 breadthFirstIndex;
	u32 params;
};

const LightTreeLutItem* getLightTreeLut(u32 index);

struct ShallowLightTreeNode
{
	float center;
	float radius;
};

struct alignas(16) PackedLightTreeNode
{
	float center;
	float radius;
	u32   lightOffset;
	u32   params;
};

inline u32 packNodeParams(u32 lightCount, u32 isLeaf, u32 skipCount)
{
	return (isLeaf << 31) | (lightCount << 15) | (skipCount & 0x7FFF);
}

inline u32 getLightCount(const PackedLightTreeNode& node) { return (node.params >> 15) & 0xFFFF; }

inline u32 getSkipCount(const PackedLightTreeNode& node) { return node.params & 0x7FFF; }

inline bool getIsLeaf(const PackedLightTreeNode& node) { return (node.params & 0x80000000) != 0; }

static_assert(sizeof(PackedLightTreeNode) == 16, "Packed node must be exactly 16 bytes");

struct TiledLightTreeBuildResult
{
	u32    visibleLightCount                 = 0;
	double lightCullTime                     = 0;
	double lightSortTime                     = 0;
	double lightAssignTime                   = 0;
	double buildTreeTime                     = 0;
	double uploadTime                        = 0;
	double buildTotalTime                    = 0;
	float  hierarchicalCullingDepthThreshold = 0;
	u32    sliceCount                        = 0;
	u32    treeCellCount                     = 0;
	u32    listCellCount                     = 0;

	u32 totalDataSize = 0;
	u32 lightDataSize = 0;
	u32 treeDataSize  = 0;

	GfxBuffer lightTreeBuffer;
	GfxBuffer lightIndexBuffer;
	GfxBuffer lightTileInfoBuffer;
};

struct TiledLightTreeBuildParams : CommonLightBuildParams
{
	int   targetLightsPerLeaf = 6;   // experimentally found to be a good default
	float lightTreeHeuristic = 1.0f; // use light tree if average light extents is less than this proportion of the cell
	                                 // extents (only used in hybrid mode)
	bool useClippedLightExtents = false; // controls whether the full light depth extents are used for light tree
	                                     // heuristic or only the part that overlaps with the cell (only used in hybrid
	                                     // mode)
	u32  maxLeafNodes          = 64;
	u32  useTileFrustumCulling = 0;
	bool useShallowTree        = false;
};

struct TreeBuildParams
{
	u32   maxLeafNodes     = 64;
	u32   minLightsPerLeaf = 8;
	float minNodeExtent    = 4.0f;
};

class TiledLightTreeBuilderBase
{
protected:
	struct alignas(16) LightGridCell
	{
		u32 lightOffset;
		u32 lightCount;
		u32 treeOffset;
		u32 treeNodeCount;
	};

public:
	static constexpr u32 MaxBottomUpTreeLevels = 7;
	static constexpr u32 MaxLeafNodes =
	    1 << (MaxBottomUpTreeLevels - 1); // Limit tree size something sensible that fits into LDS;
	static constexpr u32 MaxTotalNodes = MaxLeafNodes * 2 - 1;
};

class TiledLightTreeBuilder : public TiledLightTreeBuilderBase
{
public:
	TiledLightTreeBuilder(u32 maxLights);

	AlignedArray<u32>                         m_visibleLightIndices;
	AlignedArray<LightDepthInterval>          m_lightIntervals;
	AlignedArray<LightTileScreenSpaceExtents> m_lightScreenSpaceExtents;
	std::vector<LightTreeNode>                m_tempLightTree; // temporary tree
	std::vector<PackedLightTreeNode>          m_gpuLightTree;
	std::vector<LightSource>                  m_gpuLights;
	std::vector<u16>                          m_gpuLightIndices;

	GfxBufferRef m_lightIndexBuffer;
	GfxBufferRef m_lightTreeBuffer;
	GfxBufferRef m_lightTileInfoBuffer;

	GfxBufferRef m_lightDepthIntervalBuffer;
	GfxBufferRef m_lightDepthIntervalIndexBuffer;

	AlignedArray<LightGridCell> m_lightGrid;
	AlignedArray<u16>           m_tileIntervalIndices;
	AlignedArray<u16>           m_tileIntervalIndicesSorted;

	std::vector<u16> m_cellLightCount;
	std::vector<u16> m_tileLightCount;

	std::vector<u32> m_treeBuildQueue;

	const u32 m_maxLights;

	TileFrustumCache m_tileFrustumCache;

	TiledLightTreeBuildResult build(GfxContext* ctx,
	    const Camera&                           camera,
	    const std::vector<LightSource>&         viewSpaceLights,
	    const TiledLightTreeBuildParams&        buildParams);
};
