#version 430

layout(location=0) in vec2 frag_font_atlas_uv;

layout(location=0) out vec4 color;

uniform sampler2D font_atlas;

void main()
{
	color = texture(font_atlas, frag_font_atlas_uv);
}
