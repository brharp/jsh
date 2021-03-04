#define WS_ISTEXT(c) (((c) & 0x0F) == 0x01)
#define WS_ISFIN(c) (((c) & 0x80))
#define WS_ISMASK(c) ((c) & 0x80)

/*
 * websocket_fdopen opens a file descriptor (which
 * should be a socket) as a websocket.
 *
 * websocket_read copies up to n bytes from the input
 * queue to a caller supplied buffer. If no bytes are
 * available, it refills the input buffer.
 *
 * websocket_write copies up to n bytes from a caller
 * supplied buffer to the output queue. When the output
 * buffer is full, it is written out.
 *
 * websocket_flush immediately writes the output buffer
 * to the underlying socket descriptor.
 *
 * It is up to the application to set a maximum
 * message size. 
 *
 * Both websocket_read() and websocket_write() need to
 * allocate temporary buffers that are WEBSOCKET_HEADER_SIZE
 * bytes larger than those supplied by the caller.
 */ 
typedef struct websocket_ WEBSOCKET;

// WEBSOCKET *websocket_open(int fd);
// int websocket_read(WEBSOCKET *, void *, int);
// int websocket_write(WEBSOCKET *, void *, int);
// int websocket_fileno(WEBSOCKET *);
// int websocket_peek(WEBSOCKET *);
// int websocket_close(WEBSOCKET *);
// int websocket_send(WEBSOCKET *, void *, int, int);
// int websocket_recv(WEBSOCKET *, void *, int, int);
// void wssetbuf(void *, int);

// websockt wsfdopen(int fd);
// int wsread(websockt, void *, int);
// int wswrite(websockt, void *, int);
// int wssend();
// int wsrecv();
// int wsfileno();
// int wspeek(websockt);
// void wssetbuf(void *, int);

WEBSOCKET *ws_open(int port);
int ws_close(WEBSOCKET *ws);
void ws_flush(WEBSOCKET *ws);
int ws_write(WEBSOCKET *p, const void *buf, size_t count);
int ws_read(WEBSOCKET *p, void *buf, size_t count);
int ws_getc(WEBSOCKET *p);
int ws_putc(int c, WEBSOCKET *p);
