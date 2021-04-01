
websockd: websockd.c websocket.c websocket.h base64.c
	$(CC) -g -o $@ $^ -lssl -lcrypto

graph: graph.c
	$(CC) -g -o $@ $^ -lm

pretty:
	indent -kr *.c *.h

clean:
	rm -f jsh

install: websockd graph
	install graph /usr/sbin
	install websockd /usr/sbin
	/etc/rc.d/rc.inetd restart

