#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <stdarg.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/select.h>
#include <signal.h>
#include <unistd.h>
#include "base64.h"
#include "websocket.h"

#define IOBUFSZ 4096
#define PORT    8001
#define QLEN    10

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

#define MAX(x, y) (x > y ? x : y)

void sig_pipe(int signo)
{
    printf("SIGPIPE\n");
    exit(EXIT_FAILURE);
}


int initserver(int type, const struct sockaddr *addr, socklen_t alen,
	       int qlen)
{
    int fd, reuse = 1;

    if ((fd = socket(addr->sa_family, type, 0)) < 0)
	error(EXIT_FAILURE, errno, "socket");
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
	error(EXIT_FAILURE, errno, "setsockopt");
    if (bind(fd, addr, alen) < 0)
	error(EXIT_FAILURE, errno, "bind");
    if (type == SOCK_STREAM || type == SOCK_SEQPACKET)
	if (listen(fd, qlen) < 0)
	    error(EXIT_FAILURE, errno, "listen");
    return fd;
}


struct buf {
	char *base;
	char *ptr;
	int  cnt;
};

void service(int fd)
{
    int req[2], res[2];		/* request and response pipes */
    int pid, nfds, nr;
    fd_set readers;
    char buf[IOBUFSZ];
    struct buf line = {0};
    int i, n;

    websocket_open(fd);

    if (pipe(req) < 0 || pipe(res) < 0)
	error(EXIT_FAILURE, errno, "pipe");

    if ((pid = fork()) == 0) {	/* child */
	close(req[1]);
	close(res[0]);

	if (dup2(req[0], STDIN_FILENO) != STDIN_FILENO)
	    error(EXIT_FAILURE, errno, "dup2");
	close(req[0]);

	if (dup2(res[1], STDOUT_FILENO) != STDOUT_FILENO)
	    error(EXIT_FAILURE, errno, "dup2");
	close(res[1]);

	if (execlp("./graph", "graph", (char *) 0) < 0)
	    error(EXIT_FAILURE, errno, "execlp");
    } else if (pid > 0) {	/* parent */
	close(req[0]);
	close(res[1]);
	for (;;) {
	    FD_ZERO(&readers);
	    FD_SET(fd, &readers);
	    FD_SET(res[0], &readers);
	    nfds = MAX(res[0], fd) + 1;
	    nfds = select(nfds, &readers, NULL, NULL, NULL);
	    if (nfds < 0)
		error(EXIT_FAILURE, errno, "select");
	    /* Read from pipe. */
	    if (FD_ISSET(res[0], &readers)) {
		if ((nr = read(res[0], buf, IOBUFSZ)) < 0)
			error(EXIT_FAILURE, errno, "read");
		for (i = 0; i < nr; i++) {
			if (--line.cnt < 0) {
				n = line.ptr - line.base;
				line.base = realloc(line.base, n + IOBUFSZ);
				line.ptr = line.base + n;
				line.cnt = IOBUFSZ;
			}
			if ((*line.ptr++ = buf[i]) == '\n') {
				n = line.ptr - line.base;
				write(STDERR_FILENO, "\nSending: ", 10);
				write(STDERR_FILENO, line.base, n);
				if (websocket_write(fd, line.base, n - 1) < 0)
					error(EXIT_FAILURE, 0, "websocket_write");
				line.ptr = line.base;
				line.cnt += n;
			}
		}
	    }
	    if (FD_ISSET(fd, &readers)) {
		if ((nr = websocket_read(fd, buf, IOBUFSZ-1)) < 0)
		    error(EXIT_FAILURE, 0, "websocket_read");
		buf[nr++] = '\n';
		write(STDERR_FILENO, buf, nr);
		if (write(req[1], buf, nr) < 0)
			error(EXIT_FAILURE, errno, "write");
	    }
	}
    } else {
	error(EXIT_FAILURE, errno, "fork");
    }

    close(res[0]);
    close(req[1]);

    websocket_close(fd);
}

void startup(int sockfd)
{
    int clfd, status;
    pid_t pid;

    for (;;) {
	fprintf(stderr, "Waiting for connection...");
	clfd = accept(sockfd, NULL, NULL);
	if (clfd < 0)
	    error(1, errno, "accept");
	fprintf(stderr, "accepted.\n");
	service(clfd);
	close(clfd);
    }
}


int main(int argc, char *argv[])
{
    struct addrinfo *ailist, *aip;
    struct addrinfo hint;
    int sockfd, err, n;
    char *host;

    if (argc != 1) {
	fprintf(stderr, "usage: %s\n", argv[0]);
	exit(1);
    }
#ifdef _SC_HOST_NAME_MAX
    n = sysconf(_SC_HOST_NAME_MAX);
    if (n < 0)
#endif
	n = HOST_NAME_MAX;
    host = malloc(n);
    if (host == NULL)
	error(1, errno, "malloc");
    if (gethostname(host, n) < 0)
	error(1, errno, "gethostname");
    hint.ai_flags = AI_CANONNAME;
    hint.ai_family = 0;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = 0;
    hint.ai_addrlen = 0;
    hint.ai_canonname = NULL;
    hint.ai_addr = NULL;
    hint.ai_next = NULL;
    if ((err = getaddrinfo(host, "8001", &hint, &ailist)) != 0)
	error(1, 0, "getaddrinfo: %s", gai_strerror(err));
    for (aip = ailist; aip != NULL; aip = aip->ai_next) {
	if ((sockfd =
	     initserver(SOCK_STREAM, aip->ai_addr, aip->ai_addrlen,
			QLEN)) >= 0) {
	    startup(sockfd);
	    exit(0);
	}
    }
    exit(1);
}
