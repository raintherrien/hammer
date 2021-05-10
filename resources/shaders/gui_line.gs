#version 430

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location=0) in float gs_thickness[];
layout(location=1) in vec4  gs_rgba[];

layout(location=0) out vec4 fs_rgba;

uniform mat4 ortho;

void main()
{
	// determine screen-space normal
	vec4 sp0 = gl_in[0].gl_Position;
	vec4 sp1 = gl_in[1].gl_Position;
	vec2 v = normalize(sp0.xy - sp1.xy);
	vec2 n = vec2(-v.y, v.x); // 2D normal of unit is perpendicular

	fs_rgba = gs_rgba[0];
	gl_Position = ortho * (sp0 + vec4(n * gs_thickness[0] / 2, 0, 0));
	EmitVertex();
	gl_Position = ortho * (sp0 - vec4(n * gs_thickness[0] / 2, 0, 0));
	EmitVertex();

	fs_rgba = gs_rgba[1];
	gl_Position = ortho * (sp1 + vec4(n * gs_thickness[1] / 2, 0, 0));
	EmitVertex();
	gl_Position = ortho * (sp1 - vec4(n * gs_thickness[1] / 2, 0, 0));
	EmitVertex();

	EndPrimitive();
}
