#include "download-txz.h"
#include "xz.h"
#include <stdio.h>
#include <string.h>

struct decode_stream {
	struct xz_dec *xz;
	struct xz_buf b;
	uint8_t buf[256 * 1024];
	int finished;
	size_t used;
};

struct decode_stream *open_decode_stream() {
	static struct decode_stream ds;
	if (!ds.xz) {
		xz_crc32_init();
		xz_crc64_init();
		ds.xz = xz_dec_init(XZ_DYNALLOC, 100 * 1024 * 1024);
		if (!ds.xz) {
			fprintf(stderr, "failed to create xz\n");
			return NULL;
		}
	} else {
		xz_dec_reset(ds.xz);
	}
	ds.b.out = ds.buf;
	ds.b.out_size = sizeof(ds.buf);
	ds.b.out_pos = 0;
	ds.finished = 0;
	return &ds;
}

uint8_t *decoded_data(struct decode_stream *ds, int *plen) {
	*plen = ds->b.out_pos - ds->used;
	return ds->buf + ds->used;
}

void decode_used(struct decode_stream *ds, int consumed) {
	ds->used += consumed;
}

int decode_finished(struct decode_stream *ds) {
	return ds->finished;
}

int decode_more(struct decode_stream *ds, const uint8_t *data, int sz) {
	if (ds->finished || !sz) {
		fprintf(stderr, "mismatch between xz size and content length\n");
		return -1;
	}

	if (ds->used && ds->used < ds->b.out_pos) {
		memmove(ds->buf, ds->buf + ds->used, ds->b.out_pos - ds->used);
	}
	ds->b.out_pos -= ds->used;
	ds->used = 0;

	ds->b.in_pos = 0;
	ds->b.in_size = sz;
	ds->b.in = data;

	enum xz_ret err = xz_dec_run(ds->xz, &ds->b);
	if (err > XZ_STREAM_END) {
		fprintf(stderr, "xz decode failed %d\n", (int)err);
		return -1;
	} else if (err == XZ_STREAM_END) {
		ds->finished = 1;
	}

	return ds->b.in_pos;
}
