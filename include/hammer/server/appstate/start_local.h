#ifndef HAMMER_SERVER_APPSTATE_START_LOCAL_H_
#define HAMMER_SERVER_APPSTATE_START_LOCAL_H_

#include <deadlock/dl.h>

dltask *appstate_start_local_enter(void *);
void    appstate_start_local_exit (dltask *);

#endif /* HAMMER_SERVER_APPSTATE_START_LOCAL_H_ */
