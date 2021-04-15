#version 430

layout(location=0) in vec2  frag_font_atlas_uv;
layout(location=1) in float frag_font_scale;

layout(location=0) out vec4 color;

uniform sampler2D font_atlas;

const float spread = 8; // XXX whatever generated with
const float smoothing = 0.5 / (spread * frag_font_scale);

// TODO: Credit, github.com/libgdx/libgdx/wiki/Distance-field-fonts
void main()
{
	float distance = texture(font_atlas, frag_font_atlas_uv).a;
	float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, distance);
	color = vec4(1,1,1,alpha);
}
