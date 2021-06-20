#version 430

layout(location=0) in  vec3  in_position;
layout(location=1) in  vec3  in_normal;
layout(location=0) out float fs_light;

uniform mat4 mvp;

const vec3 light = normalize(vec3(1, -1, 1));

void main()
{
	gl_Position = mvp * vec4(in_position, 1);
	fs_light = max(0.2, dot(in_normal, light));
}
