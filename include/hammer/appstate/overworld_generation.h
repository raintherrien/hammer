#ifndef HAMMER_APPSTATE_OVERWORLD_GENERATION_H_
#define HAMMER_APPSTATE_OVERWORLD_GENERATION_H_

#include <deadlock/dl.h>

void appstate_overworld_generation_setup(void);
void appstate_overworld_generation_teardown(void);
void appstate_overworld_generation_reset_region_selection(void);

extern dltask appstate_overworld_generation_frame;

#endif /* HAMMER_APPSTATE_OVERWORLD_GENERATION_H_ */
