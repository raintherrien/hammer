#version 430

layout(location=0) in vec3 in_position;
layout(location=1) in vec4 in_color;

layout(location=0) out vec4 fs_color;

uniform mat4 ortho;

void main()
{
	gl_Position = ortho * vec4(in_position,1);
	fs_color = in_color;
}
