#include "stream.h"
#include "bearssl_wrapper.h"

typedef struct sha256_stream sha256_stream;

struct sha256_stream {
	stream iface;
	char *hash;
	br_sha256_context ctx;
	stream *source;
};

static void close_sha256(stream* s) {
	sha256_stream *ls = (sha256_stream*)s;
	ls->source->close(ls->source);
	free(ls);
}

static const uint8_t *sha256_data(stream *s, size_t *plen, int *atend) {
	sha256_stream *ls = (sha256_stream*) s;
	const uint8_t *p = ls->source->buffered(ls->source, plen, atend);
	if (*plen == 0 && *atend) {
		uint8_t hash[br_sha256_SIZE];
		br_sha256_out(&ls->ctx, hash);
		strcpy(ls->hash, "sha256:");
		for (int i = 0; i < br_sha256_SIZE; i++) {
			sprintf(ls->hash + strlen("sha256:") + 2*i, "%02x", hash[i]);
		}
	}
	return p;
}

static void consume_sha256(stream *s, size_t consumed) {
	sha256_stream *ls = (sha256_stream*) s;
	size_t len;
	int atend;
	const uint8_t *p = ls->source->buffered(ls->source, &len, &atend);
	br_sha256_update(&ls->ctx, p, len);
	ls->source->consume(ls->source, consumed);
}

static int sha256_get_more(stream *s) {
	sha256_stream *ls = (sha256_stream*) s;
	return ls->source->get_more(ls->source);
}

stream *open_sha256_hash(stream *source, char *hash) {
	sha256_stream *ls = malloc(sizeof(sha256_stream));
	if (!ls) {
		return NULL;
	}
	br_sha256_init(&ls->ctx);
	ls->source = source;
	ls->hash = hash;
	ls->iface.close = &close_sha256;
	ls->iface.get_more = &sha256_get_more;
	ls->iface.buffered = &sha256_data;
	ls->iface.consume = &consume_sha256;
	return &ls->iface;
}
