
graph: graph.c
	$(CC) -g -o $@ $^ -lm

pretty:
	indent -kr -ts4 *.c 

clean:
	rm -f graph *~

install: graph
	install graph /usr/sbin
	mkdir -p /var/www/htdocs/graph
	cp index.html graph.js /var/www/htdocs/graph

