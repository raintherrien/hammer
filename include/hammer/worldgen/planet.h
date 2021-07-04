#ifndef HAMMER_WORLDGEN_PLANET_H_
#define HAMMER_WORLDGEN_PLANET_H_

struct climate;
struct lithosphere;
struct stream_graph;

/*
 * Planet generation is an iterative process which steps through each of these
 * stages in order.
 */
enum planet_gen_stage {
	PLANET_GEN_STAGE_NONE,
	PLANET_GEN_STAGE_LITHOSPHERE,
	PLANET_GEN_STAGE_CLIMATE,
	PLANET_GEN_STAGE_STREAM,
	PLANET_GEN_STAGE_COMPOSITE
};

/*
 * These structs are only populated during planet and region generation due to
 * their large size.
 */
struct planet_gen {
	struct climate      *climate;
	struct lithosphere  *lithosphere;
	struct stream_graph *stream;
};

/*
 * Requested by the client, sent by the server after each iteration of planet
 * generation. Packs an RGB image of the last stage iteration.
 */
struct planet_gen_payload {
	enum planet_gen_stage stage;
	int render_size;
	unsigned char render[];
};

#endif /* HAMMER_WORLDGEN_PLANET_H_ */
