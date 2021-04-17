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
#include <syslog.h>
#include "base64.h"
#include "websocket.h"

#define HEADER_MAX_LENGTH ( 16 )
#define KEY_LENGTH ( 4 )
#define MAXLINE (256)

ssize_t buffer(int fd, char **bpp, int off, int *bszp)
{
    ssize_t nr;
    char *bp = *bpp;
    int bsz = *bszp;

    if (off >= bsz) {
	bsz += BUFSIZ;
	if ((bp = realloc(*bpp, bsz)) == NULL)
	    return -1;
	*bszp = bsz;
	*bpp = bp;
    }
    if ((nr = read(fd, &bp[off], bsz - off)) > 0)
	return off + nr;
    else
	return -1;
}

int websocket_close()
{
    return 0;
}

int websocket_open()
{
    int sz, i, nr, n;
    char *buf = 0, *ptr;
    int fd = STDIN_FILENO;

    /* read headers */
    i = sz = nr = 0;
    for (;;) {
	while (i < nr - 2) {
	    if (*ptr == '\n' && *(ptr + 1) == '\r' && *(ptr + 2) == '\n') {
		ptr += 3;
		i += 3;
		goto found;
	    }
	    ptr++;
	    i++;
	}
	if (i >= nr && ((nr = buffer(fd, &buf, i, &sz)) < 0)) {
	    free(buf);
	    return -1;
	}
	ptr = &buf[i];
	//write(STDERR_FILENO, buf, nr);
    }

    char *s, *key;

found:
    /* Parse reqest */
    syslog(LOG_DEBUG, "*** HTTP REQUEST ***\n");
    for (i = 1, s = strtok(buf, "\n"); s; s = strtok(NULL, "\n"), i++) {
	/* Trim trailing carriage return */
	if (s[(n = strlen(s)) - 1] == '\r')
	    s[--n] = '\0';
	if (n == 0)
		break;
	/* Log headers for debugging */
	syslog(LOG_DEBUG, "%03d %s\n", i, s);
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
	return -1;
    }
    strcpy(data, key);
    strcat(data, magic);
    length = strlen(data);
    SHA1(data, length, hash);
    Base64encode(accept, hash, sizeof(hash));

    /* Send response */
    s = buf;
    s += sprintf(s, "HTTP/1.1 101 Switching Protocols\r\n");
    s += sprintf(s, "Upgrade: websocket\r\n");
    s += sprintf(s, "Connection: Upgrade\r\n");
    //s += sprintf(s, "Sec-WebSocket-Protocol: null\r\n");
    s += sprintf(s, "Sec-WebSocket-Accept: %s\r\n", accept);
    s += sprintf(s, "\r\n");

    write(fd, buf, s - buf);
    //write(STDERR_FILENO, buf, s - buf);

    free(buf);
    return fd;
}


int websocket_write(void *buf, size_t count)
{
    char *base;			/*  output buffer                  */
    char *ptr;			/*  next byte                      */
    char *cbuf;			/*  char pointer to client buffer  */
    int hlen;			/*  header length                  */
    int nw;			/*  number of bytes written        */
    int fd = STDOUT_FILENO;

    base = malloc(count + HEADER_MAX_LENGTH);

    if (base == NULL)
	return -1;

    ptr = base;

    *ptr++ = 0201;		/* opcode and flags */
    *ptr++ = count > 125 ? 126 : count;	/* payload length */
    if (count > 125) {
    	*ptr++ = (count >> 8) & 0377;
    	*ptr++ = count & 0377;
    }
    hlen = ptr - base;		/*  calculate header length  */

    cbuf = buf;
    while (count-- > 0)
	*ptr++ = *cbuf++;	/*  copy data to output buffer  */

    nw = write(fd, base, ptr - base);

    return nw >= hlen ? nw - hlen : -1;
}


int websocket_read(void *buf, size_t count)
{
    char *base;			// base address of input buffer
    char *ptr;			// next byte
    char *key;			// masking key
    char *cbuf;			// char pointer to client buffer
    char *bp;
    int hlen;			// header length
    int len;			// payload length 
    int nr;			// number of bytes read
    int i;
    int fd = STDIN_FILENO;

    count += HEADER_MAX_LENGTH;
    base = malloc(count);

    if (base == NULL)
	return -1;

    nr = read(fd, base, count);

    ptr = base;
    cbuf = buf;

    while (ptr - base < nr) {
        bp = ptr;
        ptr++; // skip first byte
        len = *ptr++ & 0177;
        if (len == 126) {
	    len = *ptr++;
	    len = len << 8 | *ptr++;
        }

        key = ptr;
        ptr = ptr + KEY_LENGTH;
        hlen = ptr - bp;

        syslog(LOG_ERR, "Length = %d", len);
        syslog(LOG_ERR, "Header length = %d", hlen);
        syslog(LOG_ERR, "Bytes read = %d", nr);

        for (i = 0; i < len; i++)
	    *cbuf++ = *ptr++ ^ key[i % 4];

        *cbuf = 0;
        syslog(LOG_ERR, "Data = %s", cbuf - len);
    }

    free(base);

    return cbuf - (char *)buf;
}


#ifdef TEST
int main(int argc, char *argv[])
{
}
#endif
