#version 430

layout(location=0) in vec3  in_position;
layout(location=1) in vec3  in_rgb;
layout(location=2) in float in_font_scale;
layout(location=3) in float in_font_width;
layout(location=4) in float in_font_height;
layout(location=5) in uint  in_character;

layout(location=0) out vec3  gs_rgb;
layout(location=1) out uint  gs_character;
layout(location=2) out float gs_font_scale;
layout(location=3) out float gs_font_width;
layout(location=4) out float gs_font_height;

void main()
{
	gl_Position = vec4(in_position, 1);
	gs_rgb = in_rgb;
	gs_font_scale = in_font_scale;
	gs_font_width = in_font_width;
	gs_font_height = in_font_height;
	gs_character = in_character;
}
