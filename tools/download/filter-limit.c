#include "stream.h"

typedef struct limit_stream limit_stream;

struct limit_stream {
	stream iface;
	uint64_t left;
	stream *source;
};

static uint8_t *limit_data(stream *s, int *plen, int *atend) {
	limit_stream *ls = (limit_stream*) s;
	uint8_t *ret = ls->source->buffered(ls->source, plen, atend);
	if ((uint64_t) *plen >= ls->left) {
		*atend = 1;
		*plen = (int) ls->left;
	} else {
		*atend = 0;
	}
	return ret;
}

static void consume_limit(stream *s, int consumed) {
	limit_stream *ls = (limit_stream*) s;
	ls->left -= consumed;
	ls->source->consume(ls->source, consumed);
}

static int limit_get_more(stream *s) {
	limit_stream *ls = (limit_stream*) s;
	if (!ls->left) {
		fprintf(stderr, "read over the end of a limit\n");
		return -1;
	}
	return ls->source->get_more(ls->source);
}

stream *open_limited(stream *source, uint64_t size) {
	static limit_stream ls;
	ls.source = source;
	ls.left = size;
	ls.iface.get_more = &limit_get_more;
	ls.iface.buffered = &limit_data;
	ls.iface.consume = &consume_limit;
	return &ls.iface;
}
