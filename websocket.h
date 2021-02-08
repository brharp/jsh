#define WS_ISTEXT(c) (((c) & 0x0F) == 0x01)
#define WS_ISFIN(c) (((c) & 0x80))
#define WS_ISMASK(c) ((c) & 0x80)