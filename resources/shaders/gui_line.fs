#version 430

layout(location=0) in vec4 fs_rgba;

layout(location=0) out vec4 out_color;

void main()
{
	out_color = fs_rgba;
}
