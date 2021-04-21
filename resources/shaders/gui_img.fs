#version 430

layout(location=0) in  vec2 fs_uv;
layout(location=0) out vec4 out_color;

uniform sampler2D img;

void main()
{
	out_color = texture(img, fs_uv);
}
