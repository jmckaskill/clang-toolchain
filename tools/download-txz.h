#pragma once

#include <stdint.h>

struct download_stream;
struct download_stream *open_download_stream(const char *host, const char *path);
uint8_t *downloaded_data(struct download_stream *os, int *plen);
int download_more(struct download_stream *os);
void download_used(struct download_stream *os, int consumed);
int download_finished(struct download_stream *os);

struct decode_stream;
struct decode_stream *open_decode_stream();
uint8_t *decoded_data(struct decode_stream *ds, int *plen);
int decode_more(struct decode_stream *ds, const uint8_t *data, int sz);
void decode_used(struct decode_stream *ds, int consumed);
int decode_finished(struct decode_stream *ds);

