#ifndef HAMMER_APPSTATE_GAME_H_
#define HAMMER_APPSTATE_GAME_H_

#include <deadlock/dl.h>

void appstate_game_setup(void);
void appstate_game_teardown(void);

extern dltask appstate_game_frame;

#endif /* HAMMER_APPSTATE_GAME_H_ */
