#ifndef HAMMER_APPSTATE_MAIN_MENU_H_
#define HAMMER_APPSTATE_MAIN_MENU_H_

#include <deadlock/dl.h>

void appstate_main_menu_setup(void);
void appstate_main_menu_teardown(void);

extern dltask appstate_main_menu_frame;

#endif /* HAMMER_APPSTATE_MAIN_MENU_H_ */
