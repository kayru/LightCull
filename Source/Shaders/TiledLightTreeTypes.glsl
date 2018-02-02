#ifndef TILEDLIGHTTREETYPES_GLSL
#define TILEDLIGHTTREETYPES_GLSL

struct LightTreeNode
{
	float center;
	float radius;

#if !USE_SHALLOW_TREE
	uint lightOffset;
	uint params; // 1 bit leaf flag, 16 bits light count, 15 bits skip count
#endif // !USE_SHALLOW_TREE
};

#if !USE_SHALLOW_TREE
uint getLightCount(LightTreeNode node)
{
	return (node.params >> 15) & 0xFFFF;
}

uint getSkipCount(LightTreeNode node)
{
	return node.params & 0x7FFF;
}

bool getIsLeaf(LightTreeNode node)
{
	return (node.params & 0x80000000) != 0;
}
#endif //!USE_SHALLOW_TREE

struct LightTileInfo
{
	uint lightOffset;
	uint lightCount;
	uint treeOffset;
	uint treeNodeCount;
};

struct LightDepthInterval
{
	float center;
	float radius;
	uint params; // u16 lightIndex, u8 sliceMin, u8 sliceMax
	uint padding;
};

uint getLightIndex(LightDepthInterval interval)
{
	return interval.params & 0xFFFF;
}

struct LightTreeInfo
{
	uint totalNodeCount;
	uint leafNodeCount;
};

LightTreeInfo buildLightTreeInfo(uint minLightsPerLeaf, uint maxLeafNodes, uint lightCount)
{
	LightTreeInfo result;

	if (lightCount != 0)
	{
		uint targetLeafNodeCount = divUp(lightCount, minLightsPerLeaf);
		uint leafNodeCount = min(nextPow2(targetLeafNodeCount), maxLeafNodes);

		result.leafNodeCount = leafNodeCount;
		result.totalNodeCount = leafNodeCount * 2 - 1;
	}
	else
	{
		result.leafNodeCount = 0;
		result.totalNodeCount = 0;
	}

	return result;
}

#endif // TILEDLIGHTTREETYPES_GLSL