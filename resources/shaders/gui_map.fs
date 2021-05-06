#version 430

layout(location=0) in  vec2 fs_uv;
layout(location=0) out vec4 out_color;

uniform sampler2D map;
uniform vec2 translation;
uniform vec2 scale;

void main()
{
	out_color = texture(map, (fs_uv + translation) / scale);
}
