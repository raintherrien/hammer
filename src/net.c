#include "hammer/error.h"
#include "hammer/net.h"

//_Static_assert(NETMSG_TYPES_COUNT < UINT8_MAX, "max enum netmsg_types value does not fit in uint8_t");

/*
 * Defined in either win32/net.c or posix/net.c depending on the platform.
 */
int  client_local_peek(enum netmsg_types *type, size_t *datasz);
void client_local_read(void *data, size_t datasz);
void client_local_write(enum netmsg_types type, const void *data, size_t datasz);
int  server_local_peek(enum netmsg_types *type, size_t *datasz);
void server_local_read(void *data, size_t datasz);
void server_local_write(enum netmsg_types type, const void *data, size_t datasz);

/*
 * Default functions that warn about attempted network traffic before a
 * connection is established.
 */
static int  null_peek(enum netmsg_types *type, size_t *datasz);
static void null_read(void *data, size_t datasz);
static void null_write(enum netmsg_types type, const void *data, size_t datasz);

net_peek_fn client_peek = null_peek;
net_read_fn client_read = null_read;
net_write_fn client_write = null_write;

net_peek_fn server_peek = null_peek;
net_read_fn server_read = null_read;
net_write_fn server_write = null_write;

void
client_spawn_local_server(void)
{
	client_peek = client_local_peek;
	client_read = client_local_read;
	client_write = client_local_write;
}

void
server_listen_local(void)
{
	server_peek = server_local_peek;
	server_read = server_local_read;
	server_write = server_local_write;
}

static int
null_peek(enum netmsg_types *type, size_t *datasz)
{
	(void) type;
	(void) datasz;
	errno = ENETDOWN;
	xperror("peek before connection established");
	return 0;
}

static void
null_read(void *data, size_t datasz)
{
	(void) data;
	(void) datasz;
	errno = ENETDOWN;
	xperror("read before connection established");
}

static void
null_write(enum netmsg_types type, const void *data, size_t datasz)
{
	(void) type;
	(void) data;
	(void) datasz;
	errno = ENETDOWN;
	xperror("write before connection established");
}
