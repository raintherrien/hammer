#version 430

layout(location=0) in vec3  fs_rgb;
layout(location=1) in vec2  fs_font_atlas_uv;
layout(location=2) in float fs_font_scale;
layout(location=3) in flat uint fs_character;

layout(location=0) out vec4 out_color;

uniform sampler2DArray font_atlas;

const float spread = 10; // XXX whatever generated with
const float smoothing = 0.125 / (spread * fs_font_scale);

// TODO: Credit, github.com/libgdx/libgdx/wiki/Distance-field-fonts
void main()
{
	float distance = texture(font_atlas, vec3(fs_font_atlas_uv, fs_character)).r;
	float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, distance);
	out_color = vec4(fs_rgb,alpha);
}
