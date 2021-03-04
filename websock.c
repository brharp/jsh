#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <openssl/sha.h>
#include <sys/select.h>
#include <sys/uio.h>
#include "base64.h"
#include "websocket.h"

#define _BUFSIZE 127
#define HBUFSIZE 64
#define MAXLINE 256

struct websocket_ {
	int fd;
	/* output buffer */
	char *write_base;
	char *write_ptr;
	int write_cnt;
	/* input buffer */
	char *read_base;
	char *read_ptr;
	int read_cnt;
};

WEBSOCKET *
ws_open(int port)
{
	int sockfd, newsockfd, portno;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	int n;
	WEBSOCKET *p;
	int wsinit(int);

	const char *progname = "ws_open";
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror(progname);
		exit(1);
	}
	bzero((char *)&serv_addr, sizeof(serv_addr));
	portno = port;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror(progname);
		exit(1);
	}
	listen(sockfd, 5);
	clilen = sizeof(cli_addr);
	newsockfd = accept(sockfd,
			   (struct sockaddr *)&cli_addr,
			   &clilen);
	if (newsockfd < 0) {
		perror(progname);
		exit(1);
	}
	wsinit(newsockfd);
	p = calloc(1, sizeof(*p));
	p->fd = newsockfd;
	close(sockfd);
	return p;
}

int
ws_close(WEBSOCKET *p)
{
	close(p->fd);
	free(p->read_base);
	free(p->write_base);
	free(p);
	return 0;
}

void
handshake()
{
	int		pos, avail, size, rem, nr;
	char	*buf = 0;

	/* Read HTTP request */
	for (pos = 0, avail = 0, size = 0, rem = 0;;) {
		while (pos < avail - 3) {
			if (buf[pos] == '\r' && buf[pos+1] == '\n' && 
			    buf[pos+2] == '\r' && buf[pos+3] == '\n') {
				break;
			}
			pos++;
			rem--;
		}
		if (pos < avail - 3) {
			buf[pos+2] = '\0';
			buf[pos+3] = '\0';
			pos += 4;
			break;
		}
		if (pos + 4 >= size) {
			size += _BUFSIZE;
			char *ptr = realloc(buf, size);
			if (ptr == NULL) {
				free(buf);
				return 0;
			}
			buf = ptr;
			rem = size - avail;
		}
		nr = read(STDIN_FILENO, &buf[avail], rem);
		if (nr < 0) {
			free(buf);
			return 0;
		}
		avail += nr;
	}

	char *s, *key;
	int i;

	/* Parse reqest */
	fprintf(stderr, "*** HTTP REQUEST ***\n");
	for (i = 1, s = strtok(buf, "\n"); s; s = strtok(NULL, "\n"), i++) {
		/* Trim trailing carriage return */
		int n = strlen(s);
		if (s[n-1] == '\r') {
			s[n-1] = '\0';
		}
		/* Log headers for debugging */
		fprintf(stderr, "%03d %s\n", i, s);
		/* Record websocket key */
		if (strncasecmp(s, "Sec-WebSocket-Key: ", 19) == 0) {
			key = s + 19;
		}
	}

	unsigned char hash[SHA_DIGEST_LENGTH];
	char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	char accept[MAXLINE];
	char *data;
	size_t length;

	/* Compute accept header value */
	data = malloc(strlen(magic) + strlen(key) + 1);
	if (data == NULL) {
		free(buf);
		return 0;
	}
	strcpy(data, key);
	strcat(data, magic);
	length = strlen(data);
	SHA1(data, length, hash);
	Base64encode(accept, hash, sizeof(hash));

	/* Send response */
	char hdr[MAXLINE];
	sprintf(hdr, "HTTP/1.1 101 Switching Protocols\r\n");
	write(STDOUT_FILENO, hdr, strlen(hdr));
	sprintf(hdr, "Upgrade: websocket\r\n");
	write(STDOUT_FILENO, hdr, strlen(hdr));
	sprintf(hdr, "Connection: Upgrade\r\n");
	write(STDOUT_FILENO, hdr, strlen(hdr));
	sprintf(hdr, "Sec-WebSocket-Protocol: null\r\n");
	write(STDOUT_FILENO, hdr, strlen(hdr));
	sprintf(hdr, "Sec-WebSocket-Accept: %s\r\n", accept);
	write(STDOUT_FILENO, hdr, strlen(hdr));
	sprintf(hdr, "\r\n");
	write(STDOUT_FILENO, hdr, strlen(hdr));

	free(buf);
}

static int 
_flush(int c, WEBSOCKET *p)
{
	char             hbuf[HBUFSIZE], *hptr;
	int              hlen, n, nw;
	struct iovec     iov[2];

	/* write buffered data */
	if ((n = p->write_ptr - p->write_base) > 0) {
		hptr = hbuf;
		*hptr++ = 0201;
		*hptr++ = n;
		hlen = hptr - hbuf;
		iov[0].iov_base = hbuf;
		iov[0].iov_len = hlen;
		iov[1].iov_base = p->write_base;
		iov[1].iov_len = n;
		if ((nw = writev(p->fd, iov, 2)) != hlen + n)
			return EOF;
	}

	/* allocate buffer */
	if (p->write_base == NULL)
		if ((p->write_base = malloc(_BUFSIZE)) == NULL)
			return EOF;

	p->write_ptr = p->write_base;
	p->write_cnt = _BUFSIZE;

	return c != EOF ? ws_putc(c, p) : EOF;
}

void
ws_flush(WEBSOCKET *p)
{
	(void) _flush(EOF, p);
}

int
ws_putc(int c, WEBSOCKET *p)
{
	return --p->write_cnt >= 0 ? *p->write_ptr++ = c : _flush(c, p);
}

int
ws_write(WEBSOCKET *p, const void *buf, size_t count)
{
	int i;
	const char *cbuf = buf;
	for (i = 0; i < count; i++)
		if ((ws_putc(*cbuf++, p)) == EOF)
			break;
	return i;
}

static int
_fill(WEBSOCKET *p)
{
	int nr, hlen, len, state, mask, i;
	char c, *ptr, key[4];

	/* allocate buffer */
	if (p->read_base == NULL)
		if ((p->read_base = malloc(_BUFSIZE)) == NULL)
			return EOF;
	p->read_ptr = p->read_base;
	
	/* read frame header */
	if ((p->read_cnt = read(p->fd, p->read_base, _BUFSIZE)) < 0)
		return EOF;

	/* parse header */
	state = 0;
	while (state < 8 && --p->read_cnt >= 0) {
		c = *p->read_ptr++;
		switch (state) {
			case 0: /* flags */
				state = 1;
				break;
			case 1: /* 8 bit length */
				len = c & 0177;
				mask = c & 0200;
				state = mask ? 4 : 8;
				break;
			case 2: /* 16 bit length */
				break;
			case 3: /* 64 bit length */
				break;
			case 4: /* mask 0 */
				key[0] = c;
				state = 5;
				break;
			case 5: /* mask 1 */
				key[1] = c;
				state = 6;
				break;
			case 6: /* mask 2 */
				key[2] = c;
				state = 7;
				break;
			case 7: /* mask 3 */
				key[3] = c;
				state = 8;
				break;
		}
	}

	/* success? */
	if (state != 8)
		return EOF;

	/* reallocate buffer to accomodate complete frame */
	hlen = p->read_ptr - p->read_base;
	if ((ptr = realloc(p->read_base, hlen + len)) == NULL)
		return EOF;
	p->read_base = ptr;
	p->read_ptr = &p->read_base[hlen];

	/* read rest of frame */
	while (p->read_cnt < len) {
		if ((nr = read(p->fd, &p->read_ptr[p->read_cnt], len - p->read_cnt)) < 0)
			return EOF;
		p->read_cnt += nr;
	}

	/* mask data */
	for (i = 0; i < len; i++)
		p->read_ptr[i] ^= key[i % 4];

	return p->read_cnt;
}

int
ws_getc(WEBSOCKET *p) /* input character from websocket */
{
	return (--p->read_cnt >= 0 ? *p->read_ptr++ & 0377 : _fill(p));
}

/* ws_read - read from a websocket */
int
ws_read(WEBSOCKET *p, void *buf, size_t count)
{
	int i;
	char *cbuf = buf;

	if (p->read_cnt <= 0)
		if (_fill(p) < 0)
			return EOF;
	
	for (i = 0; i < count; i++)
		if (--p->read_cnt >= 0)
			*cbuf++ = *p->read_ptr++;
		else
			break;

	return i;
}
