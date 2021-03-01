#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <openssl/sha.h>
#include <sys/select.h>
#include <syslog.h>
#include "base64.h"

#define IOBUFSZ 4096
#define MAXLINE 1024
#define FRMSZ   10
#define PORT    8001

#define QLEN 10

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

void
system_error(const char *message)
{
	error(EXIT_FAILURE, errno, message);
}

int
initserver(int type, const struct sockaddr *addr, socklen_t alen, int qlen)
{
	int fd, reuse = 1;

	if ((fd = socket(addr->sa_family, type, 0)) < 0)
		system_error("socket");
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
		system_error("setsockopt");
	if (bind(fd, addr, alen) < 0)
		system_error("bind");
	if (type == SOCK_STREAM || type == SOCK_SEQPACKET)
		if (listen(fd, qlen) < 0)
			system_error("listen");
	return fd;
}

void
serve(int sockfd)
{
	int    clfd, status;
	pid_t  pid;

	for (;;) {
		clfd = accept(sockfd, NULL, NULL);
		if (clfd < 0)
			error(1, errno, "accept");
		if ((pid = fork()) < 0)
			error(1, errno, "fork");
		else if (pid == 0) {	/* child */
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			if (dup2(clfd, STDIN_FILENO) != STDIN_FILENO ||
					dup2(clfd, STDOUT_FILENO) != STDOUT_FILENO ||
					dup2(clfd, STDERR_FILENO) != STDERR_FILENO) {
				error(1, errno, "dup2");
			}
			close(clfd);
			execl("/usr/bin/ws", "sh", (char *) 0);
			error(1, errno, "execl");
		} else {
			close(clfd);
			waitpid(pid, &status, 0);
		}
	}
}

int main(int argc, char *argv[])
{
	struct addrinfo *ailist, *aip;
	struct addrinfo hint;
	int             sockfd, err, n;
	char            *host;

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
		if ((sockfd = initserver(SOCK_STREAM, aip->ai_addr, aip->ai_addrlen, QLEN)) >= 0) {
			serve(sockfd);
			exit(0);
		}
	}
	exit(1);
}



