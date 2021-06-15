#ifndef HAMMER_APPSTATE_WORLD_CONFIG_H_
#define HAMMER_APPSTATE_WORLD_CONFIG_H_

#include <deadlock/dl.h>

void appstate_world_config_setup(void);
void appstate_world_config_teardown(void);

extern dltask appstate_world_config_frame;

#endif /* HAMMER_APPSTATE_WORLD_CONFIG_H_ */
