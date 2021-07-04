#ifndef HAMMER_SERVER_APPSTATE_PLANET_GENERATION_H_
#define HAMMER_SERVER_APPSTATE_PLANET_GENERATION_H_

#include <deadlock/dl.h>

dltask *appstate_planet_generation_enter(void *);
void    appstate_planet_generation_exit (dltask *);

#endif /* HAMMER_SERVER_APPSTATE_PLANET_GENERATION_H_ */

