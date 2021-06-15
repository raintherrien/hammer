#version 430

layout(location=0) in  vec2 in_xz;
layout(location=0) out vec3 gs_raw_position;
layout(location=1) out vec3 gs_color;

uniform mat4 mvp;
uniform sampler2D sediment_sampler;
uniform sampler2D stone_sampler;
uniform sampler2D water_sampler;
uniform float width;
uniform float scale;

void main()
{
	const vec3 sediment_color = vec3(115, 76, 37) / 255.0;
	const vec3 stone_color = vec3(0.7);
	const vec3 water_color = vec3(96, 153, 191) / 255.0;

	float sediment = texture(sediment_sampler, in_xz / width).r;
	float stone    = texture(stone_sampler,    in_xz / width).r;
	float water    = texture(water_sampler,    in_xz / width).r;

	gs_color = mix(stone_color, sediment_color, min(1, sediment));
	gs_color = mix(gs_color, water_color, min(1, water));

	float y = (sediment + stone + water) * scale;
	gs_raw_position = vec3(in_xz[0], y, in_xz[1]);
	gl_Position = mvp * vec4(gs_raw_position, 1);
}
