#ifndef TILEDLIGHTTREESHADINGCOMMON_GLSL
#define TILEDLIGHTTREESHADINGCOMMON_GLSL

#include "LightingCommon.glsl"
#include "TiledLightTreeTypes.glsl"

layout (std430, binding = 9) readonly buffer LightTreeBuffer
{
	LightTreeNode g_lightTreeBuffer[];
};

layout (std140, binding = 10) readonly buffer LightTileInfoBuffer
{
	LightTileInfo g_lightTileInfo[];
};

layout(binding = 11, r32ui) uniform readonly uimageBuffer g_lightIndices;

LightSource getLight(uint lightIndex)
{
#if ENABLE_LIGHT_INDEX_BUFFER
	return g_lights[imageLoad(g_lightIndices, int(lightIndex)).x];
#else
	return g_lights[lightIndex];
#endif
}

#if !USE_SHALLOW_TREE
vec4 getDebugOutput(Surface surface, LightTileInfo tileInfo)
{
	if (g_debugMode == 0)
	{
		return vec4(0.0);
	}

	uint visitedNodeLimit = 500; // sanity-check

	uint visitedNodes = 0;
	uint visitedLights = 0;
	uint contributingLights = 0;

	if (g_debugMode == 1) // brute force light overdraw
	{
		visitedLights = tileInfo.lightCount;
		for (uint i=0; i<tileInfo.lightCount; ++i)
		{
			LightSource light = getLight(i + tileInfo.lightOffset);

			vec3 toLight = light.position - surface.position;
			vec3 toLightNormalized = normalize(toLight);

			float distanceToLight = length(toLight);
			if (distanceToLight < light.attenuationEnd)
			{
				contributingLights++;
			}
		}
	}
	else
	{
		uint overdraw = 0;

		uint nodeIndex = 0;

		while(nodeIndex < tileInfo.treeNodeCount && visitedNodes < visitedNodeLimit)
		{
			LightTreeNode node = g_lightTreeBuffer[tileInfo.treeOffset + nodeIndex];

			visitedNodes++;

			bool isLeaf = getIsLeaf(node);
			bool nodeHit = abs(node.center - surface.position.z) < node.radius;

			if (nodeHit)
			{
				if (isLeaf)
				{
					uint lightCount = getLightCount(node);
					if (g_debugMode == 3)
					{
						visitedLights += lightCount;
					}
					else
					{
						uint lightOffset = node.lightOffset;
						for (uint i=lightOffset; i<lightOffset + lightCount; ++i)
						{
							LightSource light = getLight(i);
							vec3 toLight = light.position - surface.position;
							float distanceToLight = length(toLight);
							if (distanceToLight < light.attenuationEnd)
							{
								contributingLights++;
							}
						}
					}
				}

				nodeIndex++;
			}
			else
			{
				nodeIndex += getSkipCount(node);
			}
		}

		return getDebugOutput(visitedNodes, visitedLights, contributingLights);
	}

}
#endif // !USE_SHALLOW_TREE

#endif // TILEDLIGHTTREESHADINGCOMMON_GLSL