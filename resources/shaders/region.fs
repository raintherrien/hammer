#version 430

layout(location=0) in  vec3 fs_normal;
layout(location=1) in  vec3 fs_color;
layout(location=0) out vec4 out_color;

const vec3 light = normalize(vec3(1, -1, 1));

void main()
{
	float lighting = max(0.2, dot(fs_normal, light));
	out_color = vec4(vec3(lighting) * fs_color, 1);
}
