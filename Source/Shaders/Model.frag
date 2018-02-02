#version 450

layout (binding = 0) uniform SceneConstants
{
	mat4 g_matViewProj;
};

layout (binding = 1) uniform InstanceConstants
{
	mat4 g_matWorld;
	vec4 g_baseColor;
	int g_useNormalMap;
};

layout (binding = 2) uniform sampler2D s_albedo;
layout (binding = 3) uniform sampler2D s_normal;
layout (binding = 4) uniform sampler2D s_roughness;

layout (location = 0) in vec2 v_tex0;
layout (location = 1) in vec3 v_nor0;
layout (location = 2) in vec3 v_tan0;
layout (location = 3) in vec3 v_bit0;

layout (location = 0) out vec4 fragColor[3];

void main()
{
	vec4 outBaseColor;
	vec4 outNormal;
	vec4 outRoughness;

	outBaseColor = g_baseColor * texture(s_albedo, v_tex0);

	if (g_useNormalMap != 0)
	{
		vec3 normal = normalize(texture(s_normal, v_tex0).xyz * 2.0 - 1.0);

		mat3 basis;
		basis[0] = normalize(v_tan0);
		basis[1] = normalize(v_bit0);
		basis[2] = normalize(v_nor0);

		outNormal.xyz = basis*normal;
	}
	else
	{
		outNormal.xyz = normalize(v_nor0);
	}

	outNormal.w = 1.0;

	outRoughness = texture(s_roughness, v_tex0).rrrr;

	fragColor[0] = outBaseColor;
	fragColor[1] = outNormal;
	fragColor[2] = outRoughness;
}
