#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include "base64.h"

#define MAXLINE 1024

int main(int argc, char *argv[])
{
	char	line[MAXLINE];
	int     n, fd1[2], fd2[2];
	pid_t   pid;

	/* complete websocket handshake */
	for (;;) {
		if (fgets(line, MAXLINE, stdin) == NULL)
			error(EXIT_FAILURE, errno, "fgets");
		n = strlen(line);
		if (line[n-1] == '\r')
			line[--n] = '\0';
		if (strncasecmp(line, "GET ", 4) == 0)
			printf("HTTP/1.1 101 Switching Protocols\r\n");
		else if (strncasecmp(line, "Upgrade: ", 9) == 0)
			printf("Upgrade: websocket\r\n");
		else if (strncasecmp(line, "Connection: ", 12) == 0)
			printf("Connection: Upgrade\r\n"0;)
		else if (strncasecmp(line, "Sec-WebSocket-Key: ", 19) == 0) {
			unsigned char hash[SHA_DIGEST_LENGTH];
			char magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
			char accept[MAXLINE];
			char *data;
			char *key = s + 19;
			data = malloc(strlen(magic) + strlen(key) + 1);
			if (data == NULL)
				error(EXIT_FAILURE, errno, "malloc");
			strcpy(data, key);
			strcat(data, magic);
			n = strlen(data);
			SHA1(data, n, hash);
			Base64encode(accept, hash, sizeof(hash));
			printf("Sec-WebSocket-Accept: %s\r\n", accept);
		}
		else if (strcmp(line, "") == 0) {
			printf("\r\n");
			break;
		}
	}
	flush(stdout);

	/* create pipes and fork */
	if (signal(SIGPIPE, sig_pipe) == SIG_ERR)
		error(EXIT_FAILURE, errno, "signal");

	if (pipe(fd1) < 0 || pipe(fd2) < 0)
		error(EXIT_FAILURE, errno, "pipe");

	if ((pid = fork()) < 0) {
		error(EXIT_FAILURE, errno, "fork");
	}
	else if (pid == 0) {    /* child */
		close(fd1[1]);
		close(fd2[0]);
		if (dup2(fd1[0], STDIN_FILENO) != STDIN_FILENO)
			error(EXIT_FAILURE, errno, "dup2");
		close(fd1[0]);
		if (dup2(fd2[1], STDOUT_FILENO) != STDOUT_FILENO)
			error(EXIT_FAILURE, errno, "dup2");
		close(fd2[1]);
		if (execlp(argv[0], argv[0], (char *) 0) < 0)
			error(EXIT_FAILURE, errno, "execlp");
	}
	else {    /* parent */
		close(fd1[0]);
		close(fd2[1]);
		
	}


	exit(EXIT_SUCCESS);
}

static void
sig_pipe(int signo)
{
	printf("SIGPIPE\n");
	exit(EXIT_FAILURE);
}

int
wsdata(int fd)
{
	unsigned char buf[IOBUFSZ], ln[126], *p, *s, key[4];
	fd_set readfds;
	int i, n, nfds, pos, len, fin, mask, opc;

	bzero(buf, IOBUFSZ);

	for (;;) {
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);
		FD_SET(fd, &readfds);
		nfds = fd + 1;
		nfds = select(nfds, &readfds, NULL, NULL, NULL);
		if (nfds < 0) {
			perror(progname);
			return 0;
		}
		/* Read from standard input. */
		if (FD_ISSET(STDIN_FILENO, &readfds)) {
			if (fgets(ln, sizeof(ln), stdin) == NULL) {
				break;
			}
			p = buf;
			*p++ = 1 << 7 | 1;
			*p++ = strlen(ln);
			for (s = ln; *s; p++, s++) {
				*p = *s;
			}
			n = p - buf;
			write(fd, buf, n);
		}
		/* Read from socket. */
		if (FD_ISSET(fd, &readfds)) {
			n = read(fd, buf, IOBUFSZ-1);
			if (n < 0) {
				perror(progname);
				return 0;
			}
			pos = 1;
			/* Parse length */
			len = buf[pos++] & 0x7F;
			if (len == 126) {
				len = buf[pos++];
				len = len << 8 | buf[pos++];
			}
			else if (len == 127) {
				len = buf[pos++];
				len = len << 8 | buf[pos++];
				len = len << 8 | buf[pos++];
				len = len << 8 | buf[pos++];
			}
			if (WS_ISMASK(buf[1])) {
				key[0] = buf[pos++];
				key[1] = buf[pos++];
				key[2] = buf[pos++];
				key[3] = buf[pos++];
				for (i = 0; pos + i < n; i++) {
					buf[pos + i] = buf[pos + i] ^ key[i % 4];
				}
			}
			write(STDOUT_FILENO, &buf[pos], n - pos);
			if (len != n - pos) {
				fprintf(stderr, "%s\n", "message truncated");
			}
		}
	}

}
