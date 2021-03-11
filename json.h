

int json_decode(const char *string, const char *fmt, ...);

/*

Example:

const char *s = "{ id: 42, name: 'forty-two' }"
json_decode(s, "{id:%s,name:%s}", &id, name);

*/
