
struct hdr
{
	const char *name;
	const char *value;
	struct hdr *next;
};

struct http
{
	enum { request, response } type;
	const char *method;
	const char *path;
	const char *version;
	struct hdr *headers;
};


struct http *http_request();

