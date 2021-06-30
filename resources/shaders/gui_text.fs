#version 430

layout(location=0) in vec4  fs_rgba;
layout(location=1) in vec3  fs_font_uvw;
layout(location=2) in float fs_font_scale;
layout(location=3) in float fs_weight;

layout(location=0) out vec4 out_color;

uniform sampler2DArray font_sampler;

const float spread = 10; // XXX whatever generated with
float smoothing = 0.125 / (spread * fs_font_scale);

// TODO: Credit, github.com/libgdx/libgdx/wiki/Distance-field-fonts
void main()
{
	float distance = texture(font_sampler, vec3(fs_font_uvw)).r;
	float alpha = smoothstep(0.5 - fs_weight - smoothing,
	                         0.5 - fs_weight + smoothing,
	                         distance);
	out_color = vec4(fs_rgba.rgb, fs_rgba.a * alpha);
}
