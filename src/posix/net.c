#include "hammer/error.h"
#include "hammer/net.h"
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>

/*
 * Our POSIX local communication uses UNIX domain sockets because the
 * performance is decent, they're easy, and when we need real networking it's
 * all file descriptors!
 *
 * TODO: sockets never freed
 */

#define CLIENT_SOCKET 0
#define SERVER_SOCKET 1

static void fd_discard(int fd, size_t datasz);
static int  fd_peek(int fd, enum netmsg_type *type, size_t *datasz);
static void fd_read(int fd, void *data, size_t datasz);

static void client_unix_discard(size_t datasz);
static int  client_unix_peek(enum netmsg_type *type, size_t *datasz);
static void client_unix_read(void *data, size_t datasz);
static void client_unix_write(enum netmsg_type type, const void *data, size_t datasz);
static void server_unix_discard(size_t datasz);
static int  server_unix_peek(enum netmsg_type *type, size_t *datasz);
static void server_unix_read(void *data, size_t datasz);
static void server_unix_write(enum netmsg_type type, const void *data, size_t datasz);

static struct {
	int fds[2];
} unix_net;

void
local_connection_init(void)
{
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, unix_net.fds) == -1) {
		xpanic("Unable to create UNIX domain socket connection");
	}

	client_discard = client_unix_discard;
	client_peek = client_unix_peek;
	client_read = client_unix_read;
	client_write = client_unix_write;

	server_discard = server_unix_discard;
	server_peek = server_unix_peek;
	server_read = server_unix_read;
	server_write = server_unix_write;
}

static void
fd_discard(int fd, size_t datasz)
{
	errno = 0;

	/* TODO: This is pretty ugly and is definitely gonna seg fault :) */
	char scratch[512];
	size_t chunks = datasz / 512 + 1;
	for (size_t i = 0; i < chunks; ++ i) {
		size_t bytes_in_chunk = 512;
		/* last chunk will be less than 512 bytes, possibly zero */
		if (i == chunks - 1)
			bytes_in_chunk = datasz % 512;
		while (bytes_in_chunk) {
			ssize_t count = read(fd, scratch, bytes_in_chunk);
			if (count == -1 || errno)
				xpanic("Failed to read from socket");
			bytes_in_chunk -= count;
		}
	}
}

static int
fd_peek(int fd, enum netmsg_type *type, size_t *datasz)
{
	errno = 0;

	int has_data = 0;
	if (ioctl(fd, FIONREAD, &has_data) == -1 || errno) {
		xperror("Unable to count socket bytes; peek may block");
		has_data = 1;
	}

	if (!has_data)
		return 0;

	errno = 0;

	char buf[NETMSG_HEADER_SZ];
	ssize_t count = recv(fd, buf, NETMSG_HEADER_SZ, MSG_PEEK);
	if (count != NETMSG_HEADER_SZ || errno) {
		xperror("Malformed netmsg header");
		return 0;
	}

	netmsg_decode_header(buf, type, datasz);
	return 1;
}

static void
fd_read(int fd, void *data, size_t datasz)
{
	errno = 0;

	char header[NETMSG_HEADER_SZ];
	ssize_t count = read(fd, header, NETMSG_HEADER_SZ);
	if (count != NETMSG_HEADER_SZ || errno)
		xpanic("Malformed netmsg header");

	errno = 0;

	char *buffer = data;
	while (datasz) {
		ssize_t count = read(fd, buffer, datasz);
		if (count == -1 || errno)
			xpanic("Failed to read from socket");
		buffer += (size_t)count;
		datasz -= (size_t)count;
	}
}

static void
fd_write(int fd, enum netmsg_type type, const void *data, size_t datasz)
{
	errno = 0;

	#define WRITE(BUF,SZ) do {                                   \
		const char *buf = BUF;                               \
		size_t sz = SZ;                                      \
		while (sz) {                                         \
			ssize_t count = write(fd, buf, sz);          \
			if (count == -1 || errno)                    \
				xpanic("Failed to write to socket"); \
			buf += (size_t)count;                        \
			sz  -= (size_t)count;                        \
		}                                                    \
	} while (0)

	/* Write header */
	char header[NETMSG_HEADER_SZ];
	netmsg_encode_header(header, type, datasz + NETMSG_HEADER_SZ);
	WRITE(header, NETMSG_HEADER_SZ);

	/* Write data */
	WRITE(data, datasz);

	#undef WRITE
}

static void
client_unix_discard(size_t datasz)
{
	fd_discard(unix_net.fds[CLIENT_SOCKET], datasz);
}

static int
client_unix_peek(enum netmsg_type *type, size_t *datasz)
{
	return fd_peek(unix_net.fds[CLIENT_SOCKET], type, datasz);
}

static void
client_unix_read(void *data, size_t datasz)
{
	fd_read(unix_net.fds[CLIENT_SOCKET], data, datasz);
}

static void
client_unix_write(enum netmsg_type type, const void *data, size_t datasz)
{
	fd_write(unix_net.fds[CLIENT_SOCKET], type, data, datasz);
}

static void
server_unix_discard(size_t datasz)
{
	fd_discard(unix_net.fds[SERVER_SOCKET], datasz);
}

static int
server_unix_peek(enum netmsg_type *type, size_t *datasz)
{
	return fd_peek(unix_net.fds[SERVER_SOCKET], type, datasz);
}

static void
server_unix_read(void *data, size_t datasz)
{
	fd_read(unix_net.fds[SERVER_SOCKET], data, datasz);
}

static void
server_unix_write(enum netmsg_type type, const void *data, size_t datasz)
{
	fd_write(unix_net.fds[SERVER_SOCKET], type, data, datasz);
}
