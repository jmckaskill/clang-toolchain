#pragma once

#include <stdint.h>

typedef struct stream stream;

struct stream {
	// returns non-zero on error
	int (*get_more)(stream*);
	uint8_t *(*buffered)(stream*, int *plen, int *atend);
	void (*consume)(stream*, int consumed);
};

stream *open_http_downloader(const char *url);
stream *open_file_downloader(const char *path);
stream *open_xz_decoder(stream *source);
stream *open_sha256_hash(stream *source);

