#ifndef HAMMER_APPSTATE_MAIN_MENU_H_
#define HAMMER_APPSTATE_MAIN_MENU_H_

#include <deadlock/dl.h>

struct rtargs;

dltask *main_menu_appstate_alloc_detached(struct rtargs *);

#endif /* HAMMER_APPSTATE_MAIN_MENU_H_ */
