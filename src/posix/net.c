#include "hammer/net.h"
#include <stdint.h>
#include <stdio.h> /* xxx debugging */

/*
 * Our POSIX local communication uses UNIX domain sockets because the
 * performance is decent and we actually **don't** want these functions to be
 * asynchronous since they'll be executing on their own dedicated threads
 * outside of the task scheduler. We want these threads to block when they're
 * waiting.
 */

int
client_local_peek(enum netmsg_types *type, size_t *datasz)
{
	(void) type;
	(void) datasz;
	printf("client_local_peek\n");
	return 0;
}

void
client_local_read(void *data, size_t datasz)
{
	(void) data;
	(void) datasz;
	printf("client_local_read\n");
}

void
client_local_write(enum netmsg_types type, const void *data, size_t datasz)
{
	(void) type;
	(void) data;
	(void) datasz;
	printf("client_local_write\n");
}

int
server_local_peek(enum netmsg_types *type, size_t *datasz)
{
	(void) type;
	(void) datasz;
	printf("server_local_peek\n");
	return 0;
}

void
server_local_read(void *data, size_t datasz)
{
	(void) data;
	(void) datasz;
	printf("server_local_read\n");
}

void
server_local_write(enum netmsg_types type, const void *data, size_t datasz)
{
	(void) type;
	(void) data;
	(void) datasz;
	printf("server_local_write\n");
}
