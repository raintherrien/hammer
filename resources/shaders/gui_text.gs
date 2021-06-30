#version 430

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location=0) in vec2  gs_dimensions[];
layout(location=1) in uint  gs_weight[];
layout(location=2) in uint  gs_style[];
layout(location=3) in uint  gs_character[];
layout(location=4) in uint  gs_color[];
layout(location=5) in float gs_font_scale[];

layout(location=0) out vec4  fs_rgba;
layout(location=1) out vec3  fs_font_uvw;
layout(location=2) out float fs_font_scale;
layout(location=3) out float fs_weight;

uniform mat4 ortho;

void main()
{
	for (int i = 0; i < gl_in.length (); ++ i) {
		float skew = (gs_style[i] & 0x1u) != 0 ? 0.25 : 0;
		vec4 v01 = vec4(0, gs_dimensions[i].y, 0, 0);
		vec4 v10 = vec4(gs_dimensions[i].x, 0, 0, 0);
		vec4 v11 = vec4(gs_dimensions[i].xy, 0, 0);

		fs_rgba = vec4(float((gs_color[i] & 0xff000000u) >> 24)/255.0,
		               float((gs_color[i] & 0x00ff0000u) >> 16)/255.0,
		               float((gs_color[i] & 0x0000ff00u) >>  8)/255.0,
		               float((gs_color[i] & 0x000000ffu) >>  0)/255.0);
		fs_font_scale = gs_font_scale[i];
		fs_weight = mix(-0.05, 0.05, float(gs_weight[i]) / 255.0);

		gl_Position = ortho * gl_in[i].gl_Position;
		fs_font_uvw = vec3(-skew, 0, gs_character[i]);
		EmitVertex();

		gl_Position = ortho * (gl_in[i].gl_Position + v01);
		fs_font_uvw = vec3(skew, 1, gs_character[i]);
		EmitVertex();

		gl_Position = ortho * (gl_in[i].gl_Position + v10);
		fs_font_uvw = vec3(1-skew, 0, gs_character[i]);
		EmitVertex();

		gl_Position = ortho * (gl_in[i].gl_Position + v11);
		fs_font_uvw = vec3(1+skew, 1, gs_character[i]);
		EmitVertex();
		EndPrimitive();
	}
}
