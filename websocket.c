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

#define HEADER_MAX_LENGTH ( 16 )
#define KEY_LENGTH ( 4 )

int
websocket_close ( int fd )
{
	return 0 ;
}

int
websocket_open ( int fd )
{
	int     pos, avail, size, rem, nr;
	char    *buf = 0;

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
				return -1;
			}
			buf = ptr;
			rem = size - avail;
		}
		nr = read(fd, &buf[avail], rem);
		if (nr < 0) {
			free(buf);
			return -1;
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
		return -1;
	}
	strcpy(data, key);
	strcat(data, magic);
	length = strlen(data);
	SHA1(data, length, hash);
	Base64encode(accept, hash, sizeof(hash));

	/* Send response */
	char *s = buf;
	s += sprintf(s, "HTTP/1.1 101 Switching Protocols\r\n");
	s += sprintf(s, "HTTP/1.1 101 Switching Protocols\r\n");
	s += sprintf(s, "Upgrade: websocket\r\n");
	s += sprintf(s, "Connection: Upgrade\r\n");
	s += sprintf(s, "Sec-WebSocket-Protocol: null\r\n");
	s += sprintf(s, "Sec-WebSocket-Accept: %s\r\n", accept);
	s += sprintf(s, "\r\n");

	write(fd, buf, s - buf);

	free(buf);
	return fd;
}


int 
websocket_write ( int fd , void * buf , size_t count )
{
	char * base ;  /*  output buffer                  */
	char * ptr  ;  /*  next byte                      */
	char * cbuf ;  /*  char pointer to client buffer  */
	int    hlen ;  /*  header length                  */
	int    nw   ;  /*  number of bytes written        */

	base = malloc ( count + HEADER_MAX_LENGTH ) ;

	if ( base == NULL )
		return -1 ;

	ptr = base ;

	* ptr ++ = 0201 ;    /*  opcode and flags         */
	* ptr ++ = count ;   /*  payload length           */
	hlen = ptr - base ;  /*  calculate header length  */

	cbuf = buf ;
	while ( count -- > 0 )
		* ptr ++ = * cbuf ++ ;	/*  copy data to output buffer  */

	nw = write ( fd , base , ptr - base ) ;

	return nw >= hlen ? nw - hlen : -1 ;
}


int
websocket_read ( int fd, void * buf, size_t count )
{
	char * base ; // base address of input buffer
	char * ptr  ; // next byte
	char * key  ; // masking key
	char * cbuf ; // char pointer to client buffer
	int    hlen ; // header length
	int    len  ; // payload length 
	int    nr   ; // number of bytes read

	count += HEADER_MAX_LENGTH ;
	base   = malloc ( count ) ;

	if ( base == NULL )
		return -1 ;

	nr  = read ( fd , base , count ) ;
	ptr = base + 1;
						// skip first byte
	len = * ptr ++ & 0177 ;
	if ( len == 126 ) {
		len = * ptr ++ ;
		len = len << 8 | * ptr ++ ;
	}

	key  = ptr ;
	ptr  = ptr + KEY_LENGTH ;
	hlen = ptr - base ;

	if ( hlen + len != nr ) {
		free ( base ) ;
		return -1 ;
	}

	cbuf = buf ;
	for ( i = 0 ; i < len ; i ++ )
		buf [ i ] = ptr [ i ] ^ key [ i % 4 ] ;

	free ( base ) ;

	return len ;
}


