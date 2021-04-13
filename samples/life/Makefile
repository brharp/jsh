
life: life.c
	cc -o life life.c

install: life
	install life /usr/sbin
	mkdir -p /var/www/htdocs/life
	cp index.html life.js /var/www/htdocs/life


