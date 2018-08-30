#include "stream.h"
#include "xz.h"
#include <stdio.h>
#include <string.h>

typedef struct xz_stream xz_stream;

struct xz_stream {
	stream iface;
	struct xz_dec *xz;
	uint8_t buf[256 * 1024];
	size_t consumed, avail;
	int finished;
	stream *source;
};

static void close_xz(stream *s) {
	xz_stream *ds = (xz_stream*)s;
	xz_dec_end(ds->xz);
	ds->source->close(ds->source);
	free(ds);
}

static const uint8_t *xz_data(stream *s, size_t *plen, int *atend) {
	xz_stream *ds = (xz_stream*) s;
	*plen = ds->avail - ds->consumed;
	*atend = ds->finished;
	return ds->buf + ds->consumed;
}

static void consume_xz(stream *s, size_t consumed) {
	xz_stream *ds = (xz_stream*) s;
	ds->consumed += consumed;
}

static int decode_xz(stream *s) {
	xz_stream *ds = (xz_stream*) s;
	if (ds->finished) {
		fprintf(stderr, "calling decode more after the decode is finished\n");
		return -1;
	}

	if (ds->consumed && ds->consumed < ds->avail) {
		memmove(ds->buf, ds->buf + ds->consumed, ds->avail - ds->consumed);
	}
	ds->avail -= ds->consumed;
	ds->consumed = 0;

	for (;;) {
		size_t len;
		int atend;
		const uint8_t *src = ds->source->buffered(ds->source, &len, &atend);

		if (len) {
			struct xz_buf b;
			b.in = src;
			b.in_pos = 0;
			b.in_size = len;
			b.out = ds->buf + ds->avail;
			b.out_pos = 0;
			b.out_size = sizeof(ds->buf) - ds->avail;

			enum xz_ret err = xz_dec_run(ds->xz, &b);
			ds->source->consume(ds->source, b.in_pos);
			ds->avail += b.out_pos;

			if (err == XZ_STREAM_END) {
				// check that upstream is done too
				ds->source->buffered(ds->source, &len, &atend);
				if (len || !atend) {
					fprintf(stderr, "xz stream finished before source\n");
					return -1;
				}
				ds->finished = 1;
				return 0;
			} else if (err) {
				fprintf(stderr, "xz decode failed %d\n", (int)err);
				ds->finished = 1;
				return -1;
			} else if (b.out_pos) {
				// we made forward progress
				return 0;
			}
		}

		// ask upstream to get more data and retry
		if (ds->source->get_more(ds->source)) {
			return -1;
		}
	}
}

stream *open_xz_decoder(stream *source) {
	xz_crc32_init();
	xz_crc64_init();
	struct xz_dec *xz = xz_dec_init(XZ_DYNALLOC, 100 * 1024 * 1024);
	if (!xz) {
		return NULL;
	}
	xz_stream *ds = malloc(sizeof(struct xz_stream));
	ds->xz = xz;
	ds->source = source;
	ds->consumed = 0;
	ds->avail = 0;
	ds->finished = 0;
	ds->iface.close = &close_xz;
	ds->iface.get_more = &decode_xz;
	ds->iface.buffered = &xz_data;
	ds->iface.consume = &consume_xz;
	return &ds->iface;
}
