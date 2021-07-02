#ifndef HAMMER_CLIENT_APPSTATE_SERVER_REGION_GEN_H_
#define HAMMER_CLIENT_APPSTATE_SERVER_REGION_GEN_H_

#include <deadlock/dl.h>

void appstate_server_region_gen_setup(void);
void appstate_server_region_gen_teardown_confirm_region(void);
void appstate_server_region_gen_teardown_discard_region(void);

extern dltask appstate_server_region_gen_frame;

#endif /* HAMMER_CLIENT_APPSTATE_SERVER_REGION_GEN_H_ */
