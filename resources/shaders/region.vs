#version 430

layout(location=0) in  vec2 in_xz;
layout(location=0) out vec3 gs_raw_position;

uniform mat4 mvp;
uniform sampler2D heightmap_sampler;
uniform float width;

void main()
{
	float y = texture(heightmap_sampler, in_xz / width).r;
	gs_raw_position = vec3(in_xz[0], y, in_xz[1]);
	gl_Position = mvp * vec4(gs_raw_position, 1);
}
