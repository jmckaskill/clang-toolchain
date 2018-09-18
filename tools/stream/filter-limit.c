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

static const uint8_t *read_limit(stream *s, size_t consume, size_t need, size_t *plen) {
	limit_stream *ls = (limit_stream*) s;
	ls->left -= consume;
	if ((uint64_t)need > ls->left) {
		need = (size_t)ls->left;
	}
	const uint8_t *ret = ls->source->read(ls->source, consume, need, plen);
	if ((uint64_t) *plen > ls->left) {
		*plen = (size_t) ls->left;
	}
	return ret;
}

stream *open_limited(stream *source, uint64_t size) {
	if (!source) {
		return NULL;
	}
	limit_stream *ls = malloc(sizeof(struct limit_stream));
	if (!ls) {
		source->close(source);
		return NULL;
	}
	ls->source = source;
	ls->left = size;
	ls->iface.close = &close_limit;
	ls->iface.read = &read_limit;
	return &ls->iface;
}
