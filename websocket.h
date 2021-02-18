#define WS_ISTEXT(c) (((c) & 0x0F) == 0x01)
#define WS_ISFIN(c) (((c) & 0x80))
#define WS_ISMASK(c) ((c) & 0x80)

typedef struct websocket_ WEBSOCKET;

WEBSOCKET *ws_open(int port);
int ws_close(WEBSOCKET *ws);
void ws_flush(WEBSOCKET *ws);
int ws_write(WEBSOCKET *p, const void *buf, size_t count);
int ws_read(WEBSOCKET *p, void *buf, size_t count);
int ws_getc(WEBSOCKET *p);
int ws_putc(int c, WEBSOCKET *p);
