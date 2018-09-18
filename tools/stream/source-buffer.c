#include "stream.h"

#ifdef WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#endif

typedef struct buf_stream buf_stream;
typedef struct map_stream map_stream;

struct buf_stream {
	stream iface;
	uint8_t *next;
	size_t remaining;
};

static void close_buffer(stream *s) {
	free(s);
}

static const uint8_t *read_buffer(stream *s, size_t consume, size_t need, size_t *plen) {
	buf_stream *bs = (buf_stream*)s;
	bs->next += consume;
	bs->remaining -= consume;
	*plen = bs->remaining;
	return bs->next;
}

stream *open_buffer_stream(const void *data, size_t size) {
	buf_stream *bs = (buf_stream*)malloc(sizeof(buf_stream));
	if (!bs) {
		return NULL;
	}
	bs->iface.close = &close_buffer;
	bs->iface.read = &read_buffer;
	bs->next = (uint8_t*)data;
	bs->remaining = size;
	return &bs->iface;
}



