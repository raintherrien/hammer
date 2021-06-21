#include "hammer/client/chunkmesh.h"
#include "hammer/error.h"
#include "hammer/glsl.h"
#include "hammer/hexagon.h"
#include "hammer/mem.h"
#include "hammer/world/chunk.h"
#include <string.h>

struct chunkmesh_renderer chunkmesh_renderer;

struct blockvertex {
	GLfloat position[3];
	GLfloat normal[3];
};

const float diagN = 0.866f; /* sqrtf(3) / 2 or sinf(60 degrees)*/
static const struct blockvertex hex_tris[20][3] = {
	{
		{ { 0, 0, -1 }, { -diagN, 0, -0.5f } },
		{ { -diagN, 0, -0.5f }, { -diagN, 0, -0.5f } },
		{ { 0, 1, -1 }, { -diagN, 0, -0.5f } },
	},
	{
		{ { -diagN, 1, -0.5f }, { 0, 0, -1 } },
		{ { -diagN, 0, -0.5f }, { 0, 0, -1 } },
		{ { -diagN, 0, 0.5f }, { 0, 0, -1 } },
	},
	{
		{ { -diagN, 1, 0.5f }, { diagN, 0, -0.5f } },
		{ { -diagN, 0, 0.5f }, { diagN, 0, -0.5f } },
		{ { 0, 0, 1 }, { diagN, 0, -0.5f } },
	},
	{
		{ { 0, 1, 1 }, { diagN, 0, 0.5f } },
		{ { 0, 0, 1 }, { diagN, 0, 0.5f } },
		{ { diagN, 0, 0.5f }, { diagN, 0, 0.5f } },
	},
	{
		{ { 0, 1, -1 }, { 0, 1, 0 } },
		{ { -diagN, 1, 0.5f }, { 0, 1, 0 } },
		{ { diagN, 1, 0.5f }, { 0, 1, 0 } },
	},
	{
		{ { diagN, 1, 0.5f }, { 0, 0, 1 } },
		{ { diagN, 0, 0.5f }, { 0, 0, 1 } },
		{ { diagN, 0, -0.5f }, { 0, 0, 1 } },
	},
	{
		{ { diagN, 1, -0.5f }, { -diagN, 0, 0.5f } },
		{ { diagN, 0, -0.5f }, { -diagN, 0, 0.5f } },
		{ { 0, 0, -1 }, { -diagN, 0, 0.5f } },
	},
	{
		{ { diagN, 0, 0.5f }, { 0, -1, 0 } },
		{ { -diagN, 0, -0.5f }, { 0, -1, 0 } },
		{ { diagN, 0, -0.5f }, { 0, -1, 0 } },
	},
	{
		{ { 0, 1, -1 }, { -diagN, 0, -0.5f } },
		{ { -diagN, 0, -0.5f }, { -diagN, 0, -0.5f } },
		{ { -diagN, 1, -0.5f }, { -diagN, 0, -0.5f } },
	},
	{
		{ { -diagN, 1, -0.5f }, { 0, 0, -1 } },
		{ { -diagN, 0, 0.5f }, { 0, 0, -1 } },
		{ { -diagN, 1, 0.5f }, { 0, 0, -1 } },
	},
	{
		{ { -diagN, 1, 0.5f }, { diagN, 0, -0.5f } },
		{ { 0, 0, 1 }, { diagN, 0, -0.5f } },
		{ { 0, 1, 1 }, { diagN, 0, -0.5f } },
	},
	{
		{ { 0, 1, 1 }, { diagN, 0, 0.5f } },
		{ { diagN, 0, 0.5f }, { diagN, 0, 0.5f } },
		{ { diagN, 1, 0.5f }, { diagN, 0, 0.5f } },
	},
	{
		{ { -diagN, 1, 0.5f }, { 0, 1, 0 } },
		{ { 0, 1, -1 }, { 0, 1, 0 } },
		{ { -diagN, 1, -0.5f }, { 0, 1, 0 } },
	},
	{
		{ { 0, 1, -1 }, { 0, 1, 0 } },
		{ { diagN, 1, 0.5f }, { 0, 1, 0 } },
		{ { diagN, 1, -0.5f }, { 0, 1, 0 } },
	},
	{
		{ { diagN, 1, 0.5f }, { 0, 1, 0 } },
		{ { -diagN, 1, 0.5f }, { 0, 1, 0 } },
		{ { 0, 1, 1 }, { 0, 1, 0 } },
	},
	{
		{ { diagN, 1, 0.5f }, { 0, 0, 1 } },
		{ { diagN, 0, -0.5f }, { 0, 0, 1 } },
		{ { diagN, 1, -0.5f }, { 0, 0, 1 } },
	},
	{
		{ { diagN, 1, -0.5f }, { -diagN, 0, 0.5f } },
		{ { 0, 0, -1 }, { -diagN, 0, 0.5f } },
		{ { 0, 1, -1 }, { -diagN, 0, 0.5f } },
	},
	{
		{ { diagN, 0, -0.5f }, { 0, -1, 0 } },
		{ { -diagN, 0, -0.5f }, { 0, -1, 0 } },
		{ { 0, 0, -1 }, { 0, -1, 0 } },
	},
	{
		{ { -diagN, 0, -0.5f }, { 0, -1, 0 } },
		{ { 0, 0, 1 }, { 0, -1, 0 } },
		{ { -diagN, 0, 0.5f }, { 0, -1, 0 } },
	},
	{
		{ { 0, 0, 1 }, { 0, -1, 0 } },
		{ { -diagN, 0, -0.5f }, { 0, -1, 0 } },
		{ { diagN, 0, 0.5f }, { 0, -1, 0 } },
	},
};

void
chunkmesh_gl_create(struct chunkmesh *m, const struct chunk *c, int cy, int cr, int cq)
{
	const size_t n = 20 * 3;
	struct blockvertex *vs = xmalloc(CHUNK_VOL * n * sizeof(*vs));
	m->vc = 0;

	for (int y = 0; y < CHUNK_LEN; ++ y)
	for (int r = 0; r < CHUNK_LEN; ++ r)
	for (int q = 0; q < CHUNK_LEN; ++ q) {
		if (!is_block_opaque(chunk_block_at(c, y, r, q)))
			continue;

		/* XXX Very easy occluded face cull */
		for (int fi = 0; fi < 20; ++ fi)
		for (int vi = 0; vi <  3; ++ vi) {
			struct blockvertex v = hex_tris[fi][vi];
			float yy = y + cy * CHUNK_LEN;
			float rr = r + cr * CHUNK_LEN;
			float qq = q + cq * CHUNK_LEN;
			v.position[0] = v.position[0] * BLOCK_HEX_SIZE + BLOCK_EUC_WIDTH * (qq + rr / 2);
			v.position[1] = v.position[1] * BLOCK_HEX_SIZE + yy;
			v.position[2] = v.position[2] * BLOCK_HEX_SIZE + rr * 0.75f * BLOCK_EUC_HEIGHT;
			vs[m->vc ++] = v;
		}
	}

	glGenVertexArrays(1, &m->vao);
	glGenBuffers(1, &m->vbo);
	glBindVertexArray(m->vao);
	glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	const size_t stride = sizeof(struct blockvertex);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid *)offsetof(struct blockvertex, position));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (GLvoid *)offsetof(struct blockvertex, normal));
	glBufferData(GL_ARRAY_BUFFER, stride * m->vc, vs, GL_STATIC_DRAW);

	free(vs);
}

void
chunkmesh_gl_destroy(struct chunkmesh *m)
{
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &m->vao);
	glDeleteBuffers(1, &m->vbo);
}

void
chunkmesh_renderer_gl_create(void)
{
	chunkmesh_renderer.shader = compile_shader_program(
	                              "resources/shaders/chunk.vs",
	                              NULL, /* no geometry shader */
	                              "resources/shaders/chunk.fs");
	if (chunkmesh_renderer.shader == 0)
		xpanic("Error creating chunk shader");
	glUseProgram(chunkmesh_renderer.shader);
	chunkmesh_renderer.uniforms.mvp = glGetUniformLocation(chunkmesh_renderer.shader, "mvp");
}

void
chunkmesh_renderer_gl_destroy(void)
{
	glDeleteProgram(chunkmesh_renderer.shader);
}
