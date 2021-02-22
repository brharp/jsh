/*
	js_write sends data over the websocket connection. It returns the number of 
	payload bytes written, which may be less than the number requested. In case 
	of a short write, the caller should call js_write again to send the 
	remaining data.
 
	js_read receives data over the websocket connection.

RETURN VALUE
	js_read() returns the length of the message. If the message is to long to
	fit in the supplied buffer, excess bytes are discarded.

	js_write() returns the number of data bytes written (not including framing.)
	This may be less than count, indicating the message was truncated.
*/


#define JS_SUCCESS    (0)
#define JS_FAILURE   (-1)

#define JS_ERR_SYSTEM    2
#define JS_ERR_NETDB     3

