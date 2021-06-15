#ifndef HAMMER_APPSTATE_REGION_GENERATION_H_
#define HAMMER_APPSTATE_REGION_GENERATION_H_

#include <deadlock/dl.h>

void appstate_region_generation_setup(void);
void appstate_region_generation_teardown_confirm_region(void);
void appstate_region_generation_teardown_discard_region(void);

extern dltask appstate_region_generation_frame;

#endif /* HAMMER_APPSTATE_REGION_GENERATION_H_ */
