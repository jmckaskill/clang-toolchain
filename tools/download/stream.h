#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#ifdef _WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#include <direct.h>
#include <shellapi.h>
#else
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

typedef struct stream stream;

struct stream {
	// returns non-zero on error
	int (*get_more)(stream*);
	uint8_t *(*buffered)(stream*, int *plen, int *atend);
	void (*consume)(stream*, int consumed);
};

#define HASH_BUFSZ 128

stream *open_http_downloader(const char *url, uint64_t *ptotal);
stream *open_file(FILE *f);
stream *open_limited(stream *source, uint64_t size);
stream *open_xz_decoder(stream *source);
stream *open_inflate(stream *source);
stream *open_sha256_hash(stream *source, char *hash);
int extract_file(stream *s, char *path, uint64_t progress_size);
int extract_zip(FILE *f, const char *dir);
int extract_tar(stream *s, const char *dir);
void make_directory(char *filepath);
char *clean_path(const char *dir, const char *name1, const char *name2);

