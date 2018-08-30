#include "stream.h"

typedef struct limit_stream limit_stream;

struct limit_stream {
	stream iface;
	uint64_t left;
	stream *source;
};

static void close_limit(stream *s) {
	// Stream limiters are used when the underlying source
	// is used across multiple stream chains. As such we 
	// do _not_ close the underlying source.
	free(s);
}

static const uint8_t *limit_data(stream *s, size_t *plen, int *atend) {
	limit_stream *ls = (limit_stream*) s;
	const uint8_t *ret = ls->source->buffered(ls->source, plen, atend);
	if ((uint64_t) *plen >= ls->left) {
		*atend = 1;
		*plen = (size_t) ls->left;
	} else {
		*atend = 0;
	}
	return ret;
}

static void consume_limit(stream *s, size_t consumed) {
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
	limit_stream *ls = malloc(sizeof(struct limit_stream));
	if (!ls) {
		return NULL;
	}
	ls->source = source;
	ls->left = size;
	ls->iface.close = &close_limit;
	ls->iface.get_more = &limit_get_more;
	ls->iface.buffered = &limit_data;
	ls->iface.consume = &consume_limit;
	return &ls->iface;
}
