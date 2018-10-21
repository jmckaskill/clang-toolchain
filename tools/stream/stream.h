#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

typedef struct stream stream;
typedef struct container container;

struct stream {
	void(*close)(stream*);
	const uint8_t *(*read)(stream*, size_t consume, size_t need, size_t *plen);
};

struct container {
	void(*close)(container*);
	int(*next_file)(container*);
	stream*(*open_file)(container*);

	const char *file_path;
	const char *link_target;
	uint64_t file_size;
	int file_mode;
};

typedef struct br_hash_class_ br_hash_class;

stream *open_http_downloader(const char *url, uint64_t *ptotal);
stream *open_file_stream(FILE *f);
stream *open_buffer_stream(const void *data, size_t size);
stream *open_limited(stream *source, uint64_t size);
stream *open_xz_decoder(stream *source);
stream *open_inflate(stream *source);
stream *open_deflate(stream *source);
stream *open_gzip(stream *source);
stream *open_hash(stream *source, const br_hash_class **vt);

container *open_zip(FILE *f);
container *open_tar(stream *s);

char *clean_path(const char *name1, const char *name2);



