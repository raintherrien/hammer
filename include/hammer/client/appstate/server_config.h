#ifndef HAMMER_CLIENT_APPSTATE_SERVER_CONFIG_H_
#define HAMMER_CLIENT_APPSTATE_SERVER_CONFIG_H_

#include <deadlock/dl.h>

dltask *appstate_server_config_enter(dltask *);
void    appstate_server_config_exit (dltask *);

#endif /* HAMMER_CLIENT_APPSTATE_SERVER_CONFIG_H_ */
