#ifndef HAMMER_CLIENT_APPSTATE_GODMODE_H_
#define HAMMER_CLIENT_APPSTATE_GODMODE_H_

#include <deadlock/dl.h>

void appstate_godmode_setup(void);
void appstate_godmode_teardown(void);

extern dltask appstate_godmode_frame;

#endif /* HAMMER_CLIENT_APPSTATE_GODMODE_H_ */
