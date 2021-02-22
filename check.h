
#define check(ptr, base, cnt) \
	(ptr != NULL && ptr >= base && ptr < base + cnt ? ptr : abort())
