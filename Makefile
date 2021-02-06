
jsh: server.c base64.c
	$(CC) -o $@ $^ -lssl -lcrypto

