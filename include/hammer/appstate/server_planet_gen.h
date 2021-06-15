#ifndef HAMMER_APPSTATE_SERVER_PLANET_GEN_H_
#define HAMMER_APPSTATE_SERVER_PLANET_GEN_H_

#include <deadlock/dl.h>

void appstate_server_planet_gen_setup(void);
void appstate_server_planet_gen_teardown(void);
void appstate_server_planet_gen_reset_region_selection(void);

extern dltask appstate_server_planet_gen_frame;

#endif /* HAMMER_APPSTATE_SERVER_PLANET_GEN_H_ */
