#ifndef HAMMER_SERVER_PLANET_H_
#define HAMMER_SERVER_PLANET_H_

struct climate;
struct lithosphere;
struct stream_graph;

/*
 * These structs are only populated during planet and region generation due to
 * their large size.
 */
struct planet {
	struct climate      *climate;
	struct lithosphere  *lithosphere;
	struct stream_graph *stream;
};

#endif /* HAMMER_SERVER_PLANET_H_ */
