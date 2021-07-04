#ifndef HAMMER_NET_H_
#define HAMMER_NET_H_

#include <stddef.h>
#include <stdint.h>

enum netmsg_type {
	NETMSG_TYPE_HEARTBEAT_SYN = 0,
	NETMSG_TYPE_HEARTBEAT_ACK,

	NETMSG_TYPE_CLIENT_QUERY_SERVER_STATUS,
	NETMSG_TYPE_SERVER_RESPONSE_SERVER_STATUS,

	NETMSG_TYPE_CLIENT_SET_SERVER_CONFIG,
	NETMSG_TYPE_SERVER_ACCEPTED_CONFIG,

	NETMSG_TYPE_CLIENT_QUERY_PLANET_GENERATION_STAGE,
	NETMSG_TYPE_SERVER_RESPONSE_PLANET_GENERATION_PAYLOAD,

	NETMSG_TYPE_COUNT
};
_Static_assert(NETMSG_TYPE_COUNT < UINT8_MAX, "max enum netmsg_type value does not fit in uint8_t");
#define NETMSG_HEADER_SZ (sizeof(uint8_t) + sizeof(size_t))

typedef void (*net_discard_fn)(size_t datasz);
typedef int  (*net_peek_fn)(enum netmsg_type *type, size_t *datasz);
typedef void (*net_read_fn)(void *data, size_t datasz);
typedef void (*net_write_fn)(enum netmsg_type type, const void *data, size_t datasz);

void netmsg_decode_header(char buf[NETMSG_HEADER_SZ], enum netmsg_type *type, size_t *datasz);
void netmsg_encode_header(char buf[NETMSG_HEADER_SZ], enum netmsg_type type, size_t datasz);

/*
 * This is a complete mock right now, only supporting local (the same process)
 * communication. This function spawns a server on the client task pool and
 * updates the client and server network function pointers.
 */
void local_connection_init(void);

/*
 * Client/server communication depends on whether we're connecting locally
 * (same process) or over a network. These function pointers are set
 * when communication is established.
 */
extern net_discard_fn client_discard;
extern net_peek_fn    client_peek;
extern net_read_fn    client_read;
extern net_write_fn   client_write;

extern net_discard_fn server_discard;
extern net_peek_fn    server_peek;
extern net_read_fn    server_read;
extern net_write_fn   server_write;

#endif /* HAMMER_NET_H_ */
