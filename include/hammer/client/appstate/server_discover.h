#ifndef HAMMER_CLIENT_APPSTATE_SERVER_DISCOVER_H_
#define HAMMER_CLIENT_APPSTATE_SERVER_DISCOVER_H_

#include <deadlock/dl.h>

dltask *appstate_server_discover_enter(void *);
void    appstate_server_discover_exit (dltask *);

#endif /* HAMMER_CLIENT_APPSTATE_SERVER_DISCOVER_H_ */
