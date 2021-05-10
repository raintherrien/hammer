#version 430

layout(location=0) in vec3  in_position;
layout(location=1) in float in_thickness;
layout(location=2) in uint  in_color;

layout(location=0) out float gs_thickness;
layout(location=1) out vec4  gs_rgba;

void main()
{
	gl_Position = vec4(in_position,1);
	gs_thickness = in_thickness;
	gs_rgba = vec4(float((in_color & 0xff000000) >> 24)/255.0,
	               float((in_color & 0x00ff0000) >> 16)/255.0,
	               float((in_color & 0x0000ff00) >>  8)/255.0,
	               float((in_color & 0x000000ff) >>  0)/255.0);
}
