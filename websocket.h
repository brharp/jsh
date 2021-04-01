
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
#include <unistd.h>

int websocket_open();
int websocket_read(void *buf, size_t count);
int websocket_write(void *buf, size_t count);
int websocket_close();
