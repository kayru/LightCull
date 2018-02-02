#version 450

layout (binding = 0) uniform SceneConstants
{
	mat4 g_matViewProj;
};

layout (binding = 1) uniform InstanceConstants
{
	mat4 g_matWorld;
	vec4 g_baseColor;
};

layout (location = 0) in vec3 a_pos0;
layout (location = 1) in vec3 a_nor0;
layout (location = 2) in vec3 a_tan0;
layout (location = 3) in vec3 a_bit0;
layout (location = 4) in vec2 a_tex0;

layout (location = 0) out vec2 v_tex0;
layout (location = 1) out vec3 v_nor0;
layout (location = 2) out vec3 v_tan0;
layout (location = 3) out vec3 v_bit0;

void main()
{
	vec3 worldPos = (vec4(a_pos0, 1) * g_matWorld).xyz;

	gl_Position = vec4(worldPos, 1) * g_matViewProj;

	v_tex0 = a_tex0;
	v_nor0 = a_nor0;
	v_tan0 = a_tan0;
	v_bit0 = a_bit0;
}
