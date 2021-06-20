#version 430

layout(location=0) in  float fs_light;
layout(location=0) out vec4  out_color;

void main()
{
	out_color = vec4(fs_light, fs_light, fs_light, 1);
}
