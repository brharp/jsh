#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "http.h"

const char *
http_method()
{
	char buf[80];
	char fmt[80];
	strcpy(buf, "");
	sprintf(fmt, "%%%lds", sizeof(buf)-1);
	scanf(fmt, buf);
	return strndup(buf, sizeof(buf));
}

const char *
http_path()
{
	return NULL;
}

const char *
http_version()
{
	return NULL;
}

struct hdr *
http_header()
{
	return NULL;
}

struct http *
http_request()
{
	struct http *ht = malloc(sizeof(*ht));

	ht->type    = request;
	ht->method  = http_method();
	ht->path    = http_path();
	ht->version = http_version();

	struct hdr *hd = http_header();
	while (hd != NULL) {
		hd->next = ht->headers;
		ht->headers = hd;
		hd = http_header();
	}

	return ht;
}

#ifdef TEST
int main (int argc, char *argv[])
{
	const char *method = http_method();
	while (method != 0 && strlen(method) > 0) {
		printf("%s\n", method);
		method = http_method();
	}
}
#endif


