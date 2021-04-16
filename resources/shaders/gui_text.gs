#version 430

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location=0) in vec3  gs_rgb[];
layout(location=1) in uint  gs_character[];
layout(location=2) in float gs_font_scale[];
layout(location=3) in float gs_font_width[];
layout(location=4) in float gs_font_height[];

layout(location=0) out vec3  fs_rgb;
layout(location=1) out vec2  fs_font_atlas_uv;
layout(location=2) out float fs_font_scale;
layout(location=3) out uint  fs_character;

uniform mat4 ortho;

void main()
{
	for (int i = 0; i < gl_in.length (); ++ i) {
		fs_rgb = gs_rgb[i];
		fs_character = gs_character[i];
		fs_font_scale = gs_font_scale[i];
		gl_Position = ortho * gl_in[i].gl_Position;
		fs_font_atlas_uv = vec2(0, 0);
		EmitVertex();
		gl_Position = ortho * (gl_in[i].gl_Position + vec4(0, gs_font_height[i], 0, 0));
		fs_font_atlas_uv = vec2(0, 1);
		EmitVertex();
		gl_Position = ortho * (gl_in[i].gl_Position + vec4(gs_font_width[i], 0, 0, 0));
		fs_font_atlas_uv = vec2(1, 0);
		EmitVertex();
		gl_Position = ortho * (gl_in[i].gl_Position + vec4(gs_font_width[i], gs_font_height[i], 0, 0));
		fs_font_atlas_uv = vec2(1, 1);
		EmitVertex();
		EndPrimitive();
	}
}
