#version 450

layout (location = 0) out vec2 v_tex0;

void main()
{
	vec3 pos;

	if      (gl_VertexIndex == 0) pos = vec3(-3.0, -1.0, 1.0);
	else if (gl_VertexIndex == 1) pos = vec3(1.0, -1.0, 1.0);
	else                          pos = vec3(1.0, 3.0, 1.0);

	gl_Position = vec4(pos, 1.0);

	v_tex0 = pos.xy / 2.0 + 0.5;
}
