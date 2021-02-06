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
#include "base64.h"

#define IOBUFSZ 4096
#define MAXLINE 1024
#define FRMSZ 10

char *progname;

int wsinit(int fd);
int wsdata(int fd);

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	int n;
	progname = argv[0];
	if (argc < 2)
	{
		fprintf(stderr, "ERROR, no port provided\n");
		exit(1);
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror(progname);
		exit(1);
	}
	bzero((char *)&serv_addr, sizeof(serv_addr));
	portno = atoi(argv[1]);
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
	wsdata(newsockfd);
	close(newsockfd);
	close(sockfd);
	return 0;
}

int
wsinit(int fd)
{
	int		pos, avail, size, rem, nr;
	char		*buf = 0;

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
			size += IOBUFSZ;
			char *ptr = realloc(buf, size);
			if (ptr == NULL) {
				free(buf);
				return 0;
			}
			buf = ptr;
			rem = size - avail;
		}
		nr = read(fd, &buf[avail], rem);
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
	write(fd, hdr, strlen(hdr));
	sprintf(hdr, "Upgrade: websocket\r\n");
	write(fd, hdr, strlen(hdr));
	sprintf(hdr, "Connection: Upgrade\r\n");
	write(fd, hdr, strlen(hdr));
	sprintf(hdr, "Sec-WebSocket-Protocol: null\r\n");
	write(fd, hdr, strlen(hdr));
	sprintf(hdr, "Sec-WebSocket-Accept: %s\r\n", accept);
	write(fd, hdr, strlen(hdr));
	sprintf(hdr, "\r\n");
	write(fd, hdr, strlen(hdr));

	free(buf);
	return 1;
}

int
wsdata(int fd)
{
	unsigned char buf[IOBUFSZ], ln[126], *p, *s;
	int i, n;

	fprintf(stderr, "jsh> ");
	while (fgets(ln, sizeof(ln), stdin) != NULL) {
		p = buf;
		*p++ = 1 << 7 | 1;
		*p++ = strlen(ln);
		for (s = ln; *s; p++, s++) {
			*p = *s;
		}
		n = p - buf;
		write(fd, buf, n);
		fprintf(stderr, "jsh> ");
	}
}
