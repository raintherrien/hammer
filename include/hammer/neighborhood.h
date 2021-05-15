#ifndef HAMMER_NEIGHBORHOOD_H_
#define HAMMER_NEIGHBORHOOD_H_

enum moore_neighbor {
	MOORE_NEIGHBOR_NW,
	MOORE_NEIGHBOR_N,
	MOORE_NEIGHBOR_NE,
	MOORE_NEIGHBOR_W,
	MOORE_NEIGHBOR_C,
	MOORE_NEIGHBOR_E,
	MOORE_NEIGHBOR_SW,
	MOORE_NEIGHBOR_S,
	MOORE_NEIGHBOR_SE,
	MOORE_NEIGHBORHOOD_SIZE
};

static const int moore_neighbor_offsets[MOORE_NEIGHBORHOOD_SIZE][2] = {
	{ -1, -1 }, {  0, -1 }, {  1, -1 },
	{ -1,  0 }, {  0,  0 }, {  1,  0 },
	{ -1,  1 }, {  0,  1 }, {  1,  1 }
};

#endif /* HAMMER_NEIGHBORHOOD_H_ */
