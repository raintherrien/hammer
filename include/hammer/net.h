#ifndef HAMMER_NET_H_
#define HAMMER_NET_H_

#include <stddef.h>

enum netmsg_types {
	NETMSG_TYPE_HEARTBEAT = 0,
	NETMSG_TYPE_COUNT
};

typedef int  (*net_peek_fn)(enum netmsg_types *type, size_t *datasz);
typedef void (*net_read_fn)(void *data, size_t datasz);
typedef void (*net_write_fn)(enum netmsg_types type, const void *data, size_t datasz);

/*
 * This is a complete mock right now, only supporting local (the same process)
 * communication. This function spawns a server on the client task pool and
 * updates the client and server network function pointers.
 */
void client_spawn_local_server(void);
void server_listen_local(void);

/*
 * Client/server communication depends on whether we're connecting locally
 * (same process) or over a network. These function pointers are set
 * when communication is established.
 */
extern net_peek_fn  client_peek;
extern net_read_fn  client_read;
extern net_write_fn client_write;

extern net_peek_fn  server_peek;
extern net_read_fn  server_read;
extern net_write_fn server_write;

#endif /* HAMMER_NET_H_ */
