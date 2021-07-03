#include "hammer/appstate.h"
#include "hammer/cli.h"
#include "hammer/client/appstate/main_menu.h"
#include "hammer/client/appstate/transitions.h"
#include "hammer/client/glthread.h"
#include "hammer/error.h"
#include <deadlock/dl.h>
#include <stdlib.h>

static struct appstate_transition client_tt[] = {
	{ -1, appstate_main_menu_enter, appstate_main_menu_exit },
	{ CLIENT_APPSTATE_TRANSITION_MAIN_MENU_CLOSE, NULL, NULL }
};

int
client_main(struct rtargs args)
{
	glthread_create();

	size_t ttsz = sizeof(client_tt) / sizeof(client_tt[0]);
	if (dlmainex(appstate_register(ttsz, client_tt), NULL, NULL, args.tc))
		xperror("Error creating deadlock scheduler");

	glthread_destroy();

	return EXIT_SUCCESS;
}
