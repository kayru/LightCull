#ifndef LIGHTINGTYPES_GLSL
#define LIGHTINGTYPES_GLSL

struct LightSource
{
	vec3 position; // view space
	float attenuationEnd;
	vec3 intensity;
	float padding1;
};

struct Surface
{
	vec3 toCameraNormalized; // view space
	vec3 position; // view space
	vec3 normal; // view space

	vec3 baseColor;
	float roughness;
	float F0;

	bool isValid;
};

struct LightTileExtents
{
	// TODO: pack as fp16
	vec2 screenSpaceMin;
	vec2 screenSpaceMax;

	// TODO: pack as u8
	uvec2 tileMin;
	uvec2 tileMax;
};

#endif // LIGHTINGTYPES_GLSL
