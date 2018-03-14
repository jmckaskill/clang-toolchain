#pragma once

#include <stdint.h>
#include <stdio.h>

typedef struct stream stream;

struct stream {
	// returns non-zero on error
	int (*get_more)(stream*);
	uint8_t *(*buffered)(stream*, int *plen, int *atend);
	void (*consume)(stream*, int consumed);
};

stream *open_http_downloader(const char *url);
stream *open_file_source(FILE *f);
stream *open_limited(stream *source, uint64_t size);
stream *open_xz_decoder(stream *source);
stream *open_inflate(stream *source);
stream *open_sha256_hash(stream *source);

