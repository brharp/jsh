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
#include "base64.h"
#include "websocket.h"

#define IOBUFSZ 4096
#define MAXLINE 1024
#define FRMSZ   10
#define PORT    8001
#define QLEN 10

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

char *progname;
char *prompt = "jsh> ";

int wsinit();
int wsdata(int, int);

int sockin, sockout;
int pipein, pipeout;

void
sig_pipe(int signo)
{
  printf("SIGPIPE\n");
  exit(EXIT_FAILURE);
}

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





/*
 * Perform websocket handshake and start local server.
 */
int
handshake()
{
  int      pos, avail, size, rem, nr;
  char     *buf = 0;

  fprintf(stderr, "Starting handshake...\n");
  
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
  return 1;
}


int
websock_connect()
{
  int     n, fd1[2], fd2[2];
  pid_t   pid;
  char    line[MAXLINE];

  handshake();

  if (signal(SIGPIPE, sig_pipe) == SIG_ERR)
    error(EXIT_FAILURE, errno, "signal");

  if (pipe(fd1) < 0 || pipe(fd2) < 0)
    error(EXIT_FAILURE, errno, "pipe");

  if ((pid = fork()) < 0) {
    error(EXIT_FAILURE, errno, "fork");
  }
  else if (pid == 0) {      /* child */
    close(fd1[1]);
    close(fd2[0]);
    if (dup2(fd1[0], STDIN_FILENO) != STDIN_FILENO)
      error(EXIT_FAILURE, errno, "dup2");
    close(fd1[0]);
    if (dup2(fd2[1], STDOUT_FILENO) != STDOUT_FILENO)
      error(EXIT_FAILURE, errno, "dup2");
    close(fd2[1]);
    if (execlp("./graph", "graph", (char *)0) < 0)
      error(EXIT_FAILURE, errno, "execlp");
  }

  close(fd1[0]);
  close(fd2[1]);

  unsigned char buf[IOBUFSZ], ln[126], *p, *s, key[4];
  fd_set readfds;
  int i, nfds, pos, len, fin, mask, opc, nr;

  bzero(buf, IOBUFSZ);

  for (;;) {
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(fd2[0], &readfds);
    nfds = fd2[0] + 1;
    nfds = select(nfds, &readfds, NULL, NULL, NULL);
    if (nfds < 0) {
      perror(progname);
      return 0;
    }
    /* Read from pipe. */
    if (FD_ISSET(fd2[0], &readfds)) {
      if ((nr = read(fd2[0], ln, sizeof(ln)-1)) < 0)
	error(EXIT_FAILURE, errno, "read");
      ln[nr] = '\0';
      p = buf;
      *p++ = 1 << 7 | 1;
      *p++ = strlen(ln);
      for (s = ln; *s; p++, s++) {
	*p = *s;
      }
      n = p - buf;
      write(STDOUT_FILENO, buf, n);
    }
    /* Read from socket. */
    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      n = read(STDIN_FILENO, buf, IOBUFSZ-1);
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
      write(STDERR_FILENO, &buf[pos], n - pos);
      write(fd1[1], &buf[pos], n - pos);
      if (len != n - pos) {
	fprintf(stderr, "%s\n", "message truncated");
	break;
      }
    }
  }

  exit(0);
}



void
startup(int sockfd)
{
  int    clfd, status;
  pid_t  pid;

  for (;;) {
    clfd = accept(sockfd, NULL, NULL);
    fprintf(stderr, "Accepted connection\n");
    if (clfd < 0)
      error(1, errno, "accept");
    if ((pid = fork()) < 0)
      error(1, errno, "fork");
    else if (pid == 0) {	/* child */
      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      //close(STDERR_FILENO);
      if (dup2(clfd, STDIN_FILENO) != STDIN_FILENO ||
	  dup2(clfd, STDOUT_FILENO) != STDOUT_FILENO) {
	  //dup2(clfd, STDERR_FILENO) != STDERR_FILENO) {
	error(1, errno, "dup2");
      }
      close(clfd);
      websock_connect();
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
      startup(sockfd);
      exit(0);
    }
  }
  exit(1);
}
