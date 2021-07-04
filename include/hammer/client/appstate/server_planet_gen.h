#ifndef HAMMER_CLIENT_APPSTATE_SERVER_PLANET_GEN_H_
#define HAMMER_CLIENT_APPSTATE_SERVER_PLANET_GEN_H_

#include <deadlock/dl.h>

dltask *appstate_server_planet_gen_entry(void *);
void    appstate_server_planet_gen_exit (dltask *);

#endif /* HAMMER_CLIENT_APPSTATE_SERVER_PLANET_GEN_H_ */
