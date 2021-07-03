#ifndef HAMMER_CLIENT_APPSTATE_MAIN_MENU_H_
#define HAMMER_CLIENT_APPSTATE_MAIN_MENU_H_

#include <deadlock/dl.h>

dltask *appstate_main_menu_enter(dltask *);
void    appstate_main_menu_exit (dltask *);

#endif /* HAMMER_CLIENT_APPSTATE_MAIN_MENU_H_ */
