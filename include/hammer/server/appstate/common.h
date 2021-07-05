#ifndef HAMMER_SERVER_APPSTATE_COMMON_H_
#define HAMMER_SERVER_APPSTATE_COMMON_H_

#include "hammer/net.h"

struct server;

int server_handle_common_netmsg(struct server *, enum netmsg_type, size_t);

#endif /* HAMMER_SERVER_APPSTATE_COMMON_H_ */
