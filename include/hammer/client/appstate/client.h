#ifndef HAMMER_APPSTATE_CLIENT_H_
#define HAMMER_APPSTATE_CLIENT_H_

#include <deadlock/dl.h>

void appstate_client_setup(void);
void appstate_client_teardown(void);

extern dltask appstate_client_frame;

#endif /* HAMMER_APPSTATE_CLIENT_H_ */
