
websockd: websockd.c base64.c
	$(CC) -g -o $@ $^ -lssl -lcrypto

graph: graph.c
	$(CC) -g -o $@ $^ -lm

clean:
	rm -f jsh
