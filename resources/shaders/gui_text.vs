#version 430

layout(location=0) in vec3  in_position;
layout(location=1) in vec2  in_dimensions;
layout(location=2) in uint  in_weight;
layout(location=3) in uint  in_style;
layout(location=4) in uint  in_character;
layout(location=5) in uint  in_color;

layout(location=0) out vec2  gs_dimensions;
layout(location=1) out uint  gs_weight;
layout(location=2) out uint  gs_style;
layout(location=3) out uint  gs_character;
layout(location=4) out uint  gs_color;
layout(location=5) out float gs_font_scale;

uniform float font_atlas_height;

void main()
{
	gl_Position = vec4(in_position,1);
	gs_dimensions = in_dimensions;
	gs_weight = in_weight;
	gs_style = in_style;
	gs_character = in_character;
	gs_color = in_color;
	gs_font_scale = in_dimensions.y / font_atlas_height;
}
