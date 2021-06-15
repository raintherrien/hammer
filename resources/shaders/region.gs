#version 430

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location=0) in  vec3 gs_raw_position[];
layout(location=1) in  vec3 gs_color[];
layout(location=0) out vec3 fs_normal;
layout(location=1) out vec3 fs_color;

void main()
{
	fs_normal = cross(gs_raw_position[2] - gs_raw_position[0],
	                  gs_raw_position[1] - gs_raw_position[0]);
	fs_normal = normalize(fs_normal);
	for(int i = 0; i < gl_in.length(); ++ i) {
		gl_Position = gl_in[i].gl_Position;
		fs_color = gs_color[i];
		EmitVertex();
	}
	EndPrimitive();
}
