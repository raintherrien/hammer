#ifndef HAMMER_APPSTATE_SERVER_CONFIG_H_
#define HAMMER_APPSTATE_SERVER_CONFIG_H_

#include <deadlock/dl.h>

void appstate_server_config_setup(void);
void appstate_server_config_teardown(void);

extern dltask appstate_server_config_frame;

#endif /* HAMMER_APPSTATE_SERVER_CONFIG_H_ */
