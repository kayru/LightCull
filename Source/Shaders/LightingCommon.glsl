#ifndef LIGHTINGCOMMON_GLSL
#define LIGHTINGCOMMON_GLSL

#include "ShaderDefines.h"
#include "Common.glsl"
#include "LightingTypes.glsl"

layout (binding = 0) uniform LightingConstants
{
	mat4 g_matView;
	mat4 g_matProjInv;

	uint g_lightCount;
	uint g_outputWidth;
	uint g_outputHeight;
	uint g_debugMode;

	uint g_nodeCount;
	uint g_offsetX;
	uint g_offsetY;
	uint g_tileSize;

	float g_lightGridDepthMin;
	float g_lightGridDepthMax;
	float g_lightGridExtents;
	uint g_lightGridSliceCount;

	vec4 g_ambientColor;

	uint g_useExponentialSlices;
	uint g_flipClipSpaceY;
	uint g_constantOne;
	uint g_useShallowTree;
};

layout (binding = 1) uniform sampler2D gbufferBaseColorImage;
layout (binding = 2) uniform sampler2D gbufferNormalImage;
layout (binding = 3) uniform sampler2D gbufferRoughnessImage;
layout (binding = 4) uniform sampler2D gbufferDepthImage;
layout (binding = 5) uniform sampler2D falseColorImage;

layout(binding = 6, rgba16f) uniform image2D outputImage;

layout (std140, binding = 7) buffer LightBuffer
{
	LightSource g_lights[]; // light sources in view space
};

vec3 positionFromDepthBuffer(vec2 uv, float depth)
{
	if (g_flipClipSpaceY != 0)
	{
		uv.y = 1.0 - uv.y;
	}

	vec4 temp = vec4(uv.xy*2.0 - 1.0, depth, 1.0) * g_matProjInv;
	return temp.xyz / temp.w;
}

float evaluateGGX(vec3 N, vec3 V, vec3 L, float roughness, float F0)
{
	float alpha = roughness*roughness;

	vec3 H = normalize(V+L);

	float dotNL = max(0.0, dot(N,L));
	float dotNV = max(0.0, dot(N,V));
	float dotNH = max(0.0, dot(N,H));
	float dotLH = max(0.0, dot(L,H));

	// D
	float alphaSqr = alpha*alpha;
	float pi = 3.14159;
	float denom = dotNH * dotNH *(alphaSqr-1.0) + 1.0;
	float D = alphaSqr/(pi * denom * denom);

	// F
	float dotLH5 = pow(1.0-dotLH,5);
	float F = F0 + (1.0-F0)*(dotLH5);

	// V
	float k = alpha/2.0;
	float vis = 1.0 / ((dotNL*k-dotNL-k)*(dotNV*k-dotNV-k));

	float specular = dotNL * D * F * vis;
	return specular;
}

float smootherstep(float edge0, float edge1, float x)
{
	x = clamp((x - edge0)/(edge1 - edge0), 0.0, 1.0);
	return x*x*x*(x*(x*6 - 15) + 10);
}

vec3 HSVtoRGB(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 overdrawPseudoColor(float overdraw, float range)
{
#if 0
	float t = (1.0-min(overdraw / range, 1.0)) * 0.65;
	return HSVtoRGB(vec3(t, 1.0, overdraw==0 ? 0.0 : 1.0));
#else
	return texture(falseColorImage, vec2(overdraw / range, 0.5)).xyz;
#endif
}

vec4 getDebugOutput(uint visitedNodes, uint visitedLights, uint contributingLights)
{
	uint value = 0;
	uint range = 100;

	switch(g_debugMode)
	{
	default:
	case 0:
		break;
	case 1: // Brute-force light overdraw
	case 2: // LightOverdraw
		value = contributingLights;
		range = 20;
		break;
	case 3: // VisitedLights
		value = visitedLights;
		range = 100;
		break;
	case 4: // LightTreeVisitedNodes
		value = visitedNodes;
		range = 64; // 64 = every leaf visited
		break;
	}

	return vec4(overdrawPseudoColor(value, range), 1.0);
}

Surface getSurface(ivec2 pixelPos)
{
	vec4 gbufferBaseColor = texelFetch(gbufferBaseColorImage, pixelPos, 0);
	vec4 gbufferRoughness = texelFetch(gbufferRoughnessImage, pixelPos, 0);
	vec3 worldNormal = normalize(texelFetch(gbufferNormalImage, pixelPos, 0).xyz);
	vec3 viewSpaceNormal = worldNormal * mat3(g_matView); // TODO: could just store view space normals in the gbuffer

	vec2 uv = vec2(pixelPos) / vec2(g_outputWidth, g_outputHeight);
	float depth = texelFetch(gbufferDepthImage, pixelPos, 0).r;
	vec3 viewSpacePosition = positionFromDepthBuffer(uv, depth);
	vec3 toCamera = -viewSpacePosition;
	float distanceToCamera = length(toCamera);

	Surface surface;

	surface.toCameraNormalized = toCamera / distanceToCamera;
	surface.position = viewSpacePosition;
	surface.baseColor = gbufferBaseColor.xyz;
	surface.normal = viewSpaceNormal;
	surface.roughness = gbufferRoughness.r;
	surface.F0 = 1.0;
	surface.isValid = depth < 1.0; // "sky" pixels are considered invalid

	return surface;
}

bool accumulateSurfaceLighting(Surface surface, LightSource light, inout vec3 result)
{
	if (abs(light.position.z - surface.position.z) > light.attenuationEnd)
	{
		return false;
	}

	vec3 toLight = light.position - surface.position;
	vec3 toLightNormalized = normalize(toLight);
	float distanceToLight = length(toLight);
	float NdotL = max(0.0, dot(surface.normal, toLightNormalized));

	if (distanceToLight > light.attenuationEnd || NdotL <= 0.0)
	{
		return false;
	}

	float distanceAttenuation = smootherstep(light.attenuationEnd, 0.0, distanceToLight) / max(0.0001, distanceToLight);

	float lightSpecular = evaluateGGX(surface.normal, surface.toCameraNormalized, toLightNormalized, surface.roughness, surface.F0);
	float lightDiffuse = NdotL;

	result += (lightDiffuse + lightSpecular) * distanceAttenuation * light.intensity;

	return true;
}

vec3 evaluateAmbientLighting(Surface surface)
{
	return g_ambientColor.xyz * dot(surface.normal, surface.toCameraNormalized);
}

uint computeExponentialSliceIndex(float depth, float maxDepth, uint sliceCount)
{
	float min_depth = log2(5.0f);
	float max_depth = log2(maxDepth);
	float scale = 1.0f / (max_depth - min_depth) * (sliceCount - 1.0f);
	float bias = 1.0f - min_depth * scale;
	uint slice = uint(max(log2(depth) * scale + bias, 0.0f));
	return min(slice, sliceCount - 1);
}

uint computeLinearSliceIndex(float depth, float maxDepth, uint sliceCount)
{
	float t = float(sliceCount) * depth / maxDepth;
	return uint(min(t, float(sliceCount - 1)));
}

uint computeSliceIndex(float depth)
{
	if (g_useExponentialSlices==1)
	{
		return computeExponentialSliceIndex(depth, g_lightGridExtents, g_lightGridSliceCount);
	}
	else
	{
		return computeLinearSliceIndex(depth, g_lightGridExtents, g_lightGridSliceCount);
	}
}

#endif // LIGHTINGCOMMON_GLSL
