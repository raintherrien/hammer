#ifndef HAMMER_APPSTATE_VIZ_CLIMATE_H_
#define HAMMER_APPSTATE_VIZ_CLIMATE_H_

#include <deadlock/dl.h>

struct lithosphere;
struct world_opts;

dltask *viz_climate_appstate_alloc_detached(const struct world_opts *,
                                            struct lithosphere *);

#endif /* HAMMER_APPSTATE_VIZ_CLIMATE_H_ */
