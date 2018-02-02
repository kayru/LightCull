#version 450

layout (binding = 0) uniform sampler2D s_depth;
layout (binding = 1) uniform sampler2D s_albedo;
layout (binding = 2) uniform sampler2D s_normals;
layout (binding = 3) uniform sampler2D s_specularRM;

layout (location = 0) in vec2 v_tex0;
layout (location = 0) out vec4 fragColor[3];

void main()
{
	// TODO: pass in transcoding parameters from the app

	float outDepth = 1.0f - texture(s_depth, v_tex0).r;

	vec4 outBaseColor = texture(s_albedo, v_tex0);
	outBaseColor.a = 1.0;

	vec4 outNormal = texture(s_normals, v_tex0);
	outNormal.xyz = outNormal.xyz * 2.0 - 1.0;
	outNormal.a = 1.0;

	vec4 outRoughness = texture(s_specularRM, v_tex0).rrrr;

	gl_FragDepth = outDepth;
	fragColor[0] = outBaseColor;
	fragColor[1] = outNormal;
	fragColor[2] = outRoughness;
}
