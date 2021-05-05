#ifndef HAMMER_APPSTATE_VIZ_STREAM_H_
#define HAMMER_APPSTATE_VIZ_STREAM_H_

#include <deadlock/dl.h>

struct climate;
struct world_opts;

dltask *viz_stream_appstate_alloc_detached(const struct world_opts *,
                                           const struct climate *);

#endif /* HAMMER_APPSTATE_VIZ_STREAM_H_ */
