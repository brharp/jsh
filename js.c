#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <openssl/sha.h>
#include "base64.h"
#include "server.h"
#include "js.h"

#define QLEN 10
#define LINE_MAX 256
#define HBUFSIZ 64

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

int    fd;
int    js_errno;
char   *buf, *bufp;
int    cnt;

static void
close_ignore_errors(int fd)
{
	int err = errno;
	close(fd);
	errno = err;
}

int
js_open()
{
	struct addrinfo *ailist, *aip, hint;
	int             i, nr, sfd, err, n, reuse = 1, cnt;
	char            *s, *host, serv[32], *key;

	/* Get or guess at max host name length. */
#ifdef _SC_HOST_NAME_MAX
	n = sysconf(_SC_HOST_NAME_MAX);
	if (n < 0)
#endif
		n = HOST_NAME_MAX;

	/* Allocate space for host and service names. */
	host = malloc(n);
	if (host == NULL) {
		js_errno = JS_ERR_SYSTEM;
		return JS_FAILURE;
	}

	/* Get local host name. */
	if (gethostname(host, n) < 0) {
		free(host);
		js_errno = JS_ERR_SYSTEM;
		return JS_FAILURE;
	}

	/* Initialize hint structure. */
	hint.ai_flags = AI_CANONNAME;
	hint.ai_family = 0;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = 0;
	hint.ai_addrlen = 0;
	hint.ai_canonname = NULL;
	hint.ai_addr = NULL;
	hint.ai_next = NULL;

	/* Get addresses of local network interfaces. */
	if ((err = getaddrinfo(host, NULL, &hint, &ailist)) != 0) {
		free(host);
		js_errno = JS_ERR_NETDB;
		return JS_FAILURE;
	}
	
	for (aip = ailist; aip != NULL; aip = aip->ai_next) {
		sfd = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
		if (sfd < 0)
			continue;
		setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
		if (bind(sfd, aip->ai_addr, aip->ai_addrlen) == 0)
			break;
		close(sfd);
	}

	if (aip == NULL) {
		free(host);
		js_errno = JS_ERR_SYSTEM;
		return JS_FAILURE;
	}

	if (listen(sfd, QLEN) < 0) {
		free(host);
		close_ignore_errors(sfd);
		js_errno = JS_ERR_SYSTEM;
		return JS_FAILURE;
	}

	/* Get name of server address. */	
	if (getnameinfo(aip->ai_addr, aip->ai_addrlen,
	                host, n-1, serv, sizeof(serv)-1, 0) < 0) {
		free(host);
		close_ignore_errors(sfd);
		js_errno = JS_ERR_NETDB;
		return JS_FAILURE;
	}

	/* Print connection information. */
	fprintf(stderr, "Waiting for connection on %s:%s...\n", host, serv);
	free(host);

	/* Wait for a connection */
	fd = accept(sfd, NULL, NULL);

	/* Close server socket. */
	close(sfd);

	/* Check for valid client socket descriptor. */
	if (fd < 0) {
		js_errno = JS_ERR_SYSTEM;
		return JS_FAILURE;
	}

	/* Read HTTP request */
	if (buf == NULL) {
		if ((buf = malloc(BUFSIZ)) == NULL) {
			close_ignore_errors(fd);
			js_errno = JS_ERR_SYSTEM;
			return JS_FAILURE;
		}
	}

	if ((cnt = read(fd, buf, BUFSIZ)) < 0) {
		close_ignore_errors(fd);
		js_errno = JS_ERR_SYSTEM;
		return JS_FAILURE;
	}

	/* Read all request headers. */
	for (;;) {
		int found = 0;
		while (cnt >= 4) {
			if (bufp[0] == bufp[2] == '\r' && 
					bufp[1] == bufp[3] == '\n') {   /* found end of headers */
				cnt -= 4;
				bufp += 4;
				found = 1;
				break;
			}
			cnt--;
			bufp++;
		}
		i = bufp - buf;
		bufp = realloc(buf, i + BUFSIZ);
		if (bufp == NULL) {
			close_ignore_errors(fd);
			js_errno = JS_ERR_SYSTEM;
			return JS_FAILURE;
		}
		buf = bufp;
		bufp = &buf[i];
		if ((nr = read(fd, bufp, BUFSIZ)) < 0) {
			close_ignore_errors(fd);
			js_errno = JS_ERR_SYSTEM;
			return JS_FAILURE;
		}
		cnt += nr;
	}

	/* Parse reqest */
	fprintf(stderr, "*** HTTP REQUEST ***\n");
	for (i = 1, s = strtok(buf, "\n"); s; s = strtok(NULL, "\n"), i++) {
		/* Trim trailing carriage return */
		int n = strlen(s);
		if (s[n-1] == '\r')
			s[--n] = '\0';
		if (n == 0)
			break;
		/* Log headers for debugging */
		fprintf(stderr, "%03d %s\n", i, s);
		/* Record websocket key */
		if (strncasecmp(s, "Sec-WebSocket-Key: ", 19) == 0) {
			key = s + 19;
		}
	}

	unsigned char hash[SHA_DIGEST_LENGTH];
	char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	char ack[LINE_MAX];
	char *data;
	size_t length;

	/* Compute accept header value */
	data = malloc(strlen(magic) + strlen(key) + 1);
	if (data == NULL) {
		close_ignore_errors(fd);
		js_errno = JS_ERR_SYSTEM;
		return JS_FAILURE;
	}
	strcpy(data, key);
	strcat(data, magic);
	length = strlen(data);
	SHA1(data, length, hash);
	Base64encode(ack, hash, sizeof(hash));
	free(data);

	/* Send response */
	char hdr[LINE_MAX];
	sprintf(hdr, "HTTP/1.1 101 Switching Protocols\r\n");
	write(fd, hdr, strlen(hdr));
	sprintf(hdr, "Upgrade: websocket\r\n");
	write(fd, hdr, strlen(hdr));
	sprintf(hdr, "Connection: Upgrade\r\n");
	write(fd, hdr, strlen(hdr));
	sprintf(hdr, "Sec-WebSocket-Protocol: null\r\n");
	write(fd, hdr, strlen(hdr));
	sprintf(hdr, "Sec-WebSocket-Accept: %s\r\n", ack);
	write(fd, hdr, strlen(hdr));
	sprintf(hdr, "\r\n");
	write(fd, hdr, strlen(hdr));

	return JS_SUCCESS;
}

int
js_close()
{
	close(fd);
}

int
js_write(const void *buf, size_t count)
{
	static char     *base;
	char            *ptr;
	int             n, nw;

	if ((ptr = realloc(base, HBUFSIZ + count)) == NULL)
		return -1;
	base = ptr;
	*ptr++ = 0201;
	*ptr++ = count;
	if (count == 126) {
		*ptr++ = count >> 8 & 0377;
		*ptr++ = count & 0377;
	}
	n = ptr - base;
	memcpy(ptr, buf, count);
	nw = write(fd, base, n + count);
	return nw < n ? -1 : nw - n;
}

int
js_read(void *buf, size_t count)
{
	static char   *base; /* TODO: use one static module level buffer */
	char          *ptr, *key;
	int           len, n, nr, i, sz;

	sz = HBUFSIZ + count;
	if ((ptr = realloc(base, sz)) == NULL)
		return -1;
	base = ptr;
	if ((nr = read(fd, base, sz)) < 0)
		return -1;
	ptr++;    /* skip first byte */
	len = *ptr++ & 0177;
	if (len == 126) {
		len = *ptr++;
		len = len << 8 | *ptr++;
	}
	key = ptr;
	ptr += 4;
	n = ptr - base;
	if (n < nr)
		return -1;
	for (i = 0; i < nr - n; i++)
		ptr[i] ^= key[i % 4];
	memcpy(buf, ptr, nr - n);
	return nr - n;
}
