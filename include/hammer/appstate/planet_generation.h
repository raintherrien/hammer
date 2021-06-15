#ifndef HAMMER_APPSTATE_PLANET_GENERATION_H_
#define HAMMER_APPSTATE_PLANET_GENERATION_H_

#include <deadlock/dl.h>

void appstate_planet_generation_setup(void);
void appstate_planet_generation_teardown(void);
void appstate_planet_generation_reset_region_selection(void);

extern dltask appstate_planet_generation_frame;

#endif /* HAMMER_APPSTATE_PLANET_GENERATION_H_ */
