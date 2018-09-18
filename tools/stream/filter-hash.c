#include "stream.h"
#include "bearssl_wrapper.h"

typedef struct hash_stream hash_stream;

struct hash_stream {
	stream iface;
	const br_hash_class **hash;
	stream *source;
	const uint8_t *data;
};

static void close_hash(stream* s) {
	hash_stream *ls = (hash_stream*)s;
	ls->source->close(ls->source);
	free(ls);
}

static const uint8_t *read_hash(stream *s, size_t consume, size_t need, size_t *plen) {
	hash_stream *ls = (hash_stream*) s;
	(*ls->hash)->update(ls->hash, ls->data, consume);
	ls->data = ls->source->read(ls->source, consume, need, plen);
	return ls->data;
}

stream *open_hash(stream *source, const br_hash_class **hash) {
	if (!source) {
		return NULL;
	}
	hash_stream *ls = calloc(1, sizeof(hash_stream));
	if (!ls) {
		source->close(source);
		return NULL;
	}
	ls->source = source;
	ls->hash = hash;
	ls->iface.close = &close_hash;
	ls->iface.read = &read_hash;
	return &ls->iface;
}
