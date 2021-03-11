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
#include "base64.h"
#include "websocket.h"

#define IOBUFSZ 4096
#define MAXLINE 1024
#define FRMSZ 10

char *progname;
char *prompt = "jsh> ";

int wsinit(int fd);
int wsdata(int fd);

int main(int argc, char *argv[])
{
    WEBSOCKET *p;
    char buf[IOBUFSZ];
    int n, port;

    progname = argv[0];

    if (argc < 2) {
	fprintf(stderr, "ERROR, no port provided\n");
	exit(1);
    }

    port = atoi(argv[1]);

    fprintf(stderr, "waiting for connection on port %d\n", port);
    if ((p = ws_open(port)) == NULL) {
	fprintf(stderr, "%s: failed to open websocket on port %d\n",
		progname, port);
	exit(EXIT_FAILURE);
    }

    while (1) {
	fprintf(stderr, "%s", prompt);
	if (fgets(buf, sizeof(buf), stdin) == NULL) {
	    break;
	}
	if (ws_write(p, buf, strlen(buf)) < strlen(buf)) {
	    fprintf(stderr, "%s: error writing to websocket\n", progname);
	    exit(EXIT_FAILURE);
	}
	ws_flush(p);
	if ((n = ws_read(p, buf, sizeof(buf))) < 0) {
	    fprintf(stderr, "%s: error reading from websocket\n",
		    progname);
	    exit(EXIT_FAILURE);
	}
	buf[n] = '\0';
	fprintf(stdout, "%s\n", buf);
    }

    ws_close(p);
    return 0;
}
