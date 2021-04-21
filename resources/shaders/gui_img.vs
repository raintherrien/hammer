#version 430

layout(location=0) in  vec2 in_xy;
layout(location=0) out vec2 fs_uv;

uniform mat4 ortho;
uniform vec3 position;
uniform vec2 dimensions;

void main()
{
	gl_Position = ortho * vec4(vec3(in_xy * dimensions,0) + position,1);
	fs_uv = in_xy;
}
