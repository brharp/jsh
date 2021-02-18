
jsh: jsh.c websock.c base64.c
	$(CC) -g -o $@ $^ -lssl -lcrypto

clean:
	rm -f jsh
