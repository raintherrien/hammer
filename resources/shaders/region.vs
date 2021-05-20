#version 430

layout(location=0) in  vec2 in_xz;
layout(location=0) out vec3 gs_raw_position;
layout(location=1) out vec3 gs_color;

uniform mat4 mvp;
uniform sampler2D heightmap_sampler;
uniform sampler2D waterheight_sampler;
uniform float width;
uniform float scale;

void main()
{
	float ty = texture(heightmap_sampler, in_xz / width).r;
	float wy = texture(waterheight_sampler, in_xz / width).r;
	gs_color = mix(vec3(1), vec3(0.376, 0.6, 0.749), min(1,wy));
	gs_raw_position = vec3(in_xz[0], (ty + wy) * scale, in_xz[1]);
	gl_Position = mvp * vec4(gs_raw_position, 1);
}
