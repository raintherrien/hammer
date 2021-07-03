#include "hammer/error.h"
#include "hammer/net.h"

/*
 * Default functions that warn about attempted network traffic before a
 * connection is established.
 */
static void null_discard(size_t datasz);
static int  null_peek(enum netmsg_type *type, size_t *datasz);
static void null_read(void *data, size_t datasz);
static void null_write(enum netmsg_type type, const void *data, size_t datasz);

net_discard_fn client_discard = null_discard;
net_peek_fn client_peek = null_peek;
net_read_fn client_read = null_read;
net_write_fn client_write = null_write;

net_discard_fn server_discard = null_discard;
net_peek_fn server_peek = null_peek;
net_read_fn server_read = null_read;
net_write_fn server_write = null_write;

void
netmsg_decode_header(char buf[NETMSG_HEADER_SZ],
                     enum netmsg_type *type,
                     size_t *datasz)
{
	/* First byte is type */
	*type = *(uint8_t *)buf;
	/* Next eight bytes are big-endian size_t */
	*datasz = ((size_t)buf[8] <<  0) |
	          ((size_t)buf[7] <<  8) |
	          ((size_t)buf[6] << 16) |
	          ((size_t)buf[5] << 24) |
	          ((size_t)buf[4] << 32) |
	          ((size_t)buf[3] << 40) |
	          ((size_t)buf[2] << 48) |
	          ((size_t)buf[1] << 56);
}

void
netmsg_encode_header(char buf[NETMSG_HEADER_SZ],
                     enum netmsg_type type,
                     size_t datasz)
{
	char *sptr = (char *)&datasz;
	buf[0] = (uint8_t)type;
	for (size_t i = 1; i < 9; ++ i)
		buf[i] = sptr[8-i];
}

static void
null_discard(size_t datasz)
{
	(void)datasz;
	xperror("net discard before connection established");
}

static int
null_peek(enum netmsg_type *type, size_t *datasz)
{
	(void) type;
	(void) datasz;
	errno = ENETDOWN;
	xperror("net peek before connection established");
	return 0;
}

static void
null_read(void *data, size_t datasz)
{
	(void) data;
	(void) datasz;
	errno = ENETDOWN;
	xperror("net read before connection established");
}

static void
null_write(enum netmsg_type type, const void *data, size_t datasz)
{
	(void) type;
	(void) data;
	(void) datasz;
	errno = ENETDOWN;
	xperror("net write before connection established");
}
