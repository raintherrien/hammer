#version 430

layout(location=0) in vec3 position;
layout(location=1) in vec2 font_atlas_uv;

layout(location=0) out vec2 frag_font_atlas_uv;

uniform mat4 ortho;

void main()
{
	gl_Position = ortho * vec4(position, 1);
	frag_font_atlas_uv = font_atlas_uv;
}
