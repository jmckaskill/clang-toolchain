#include "stream.h"
#include "xz.h"
#include <stdio.h>
#include <string.h>

typedef struct xz_stream xz_stream;

struct xz_stream {
	stream iface;
	struct xz_dec *xz;
	int finished;
	stream *source;
	struct xz_buf buf;
	size_t consumed;
};

static void close_xz(stream *s) {
	xz_stream *ds = (xz_stream*)s;
	xz_dec_end(ds->xz);
	ds->source->close(ds->source);
	free(ds->buf.out);
	free(ds);
}

static const uint8_t *read_xz(stream *s, size_t consume, size_t need, size_t *plen) {
	xz_stream *ds = (xz_stream*)s;

	// see if we can handle the request with the existing buffer
	ds->consumed += consume;
	if (ds->finished || ds->consumed + need <= ds->buf.out_pos) {
		*plen = ds->buf.out_pos - ds->consumed;
		return ds->buf.out + ds->consumed;
	}

	// compress the buffer
	if (ds->consumed && ds->consumed < ds->buf.out_pos) {
		memmove(ds->buf.out, ds->buf.out + ds->consumed, ds->buf.out_pos - ds->consumed);
	}
	ds->buf.out_pos -= ds->consumed;
	ds->consumed = 0;

	do {
		if (ds->buf.in_pos < ds->buf.in_size) {
			// we need to process the data we have
			if (ds->buf.out_pos == ds->buf.out_size) {
				size_t bufsz = ds->buf.out_size + 32 * 1024;
				uint8_t *buf = realloc(ds->buf.out, bufsz);
				if (!buf) {
					goto err;
				}
				ds->buf.out = buf;
				ds->buf.out_size = bufsz;
			}

			size_t start_out = ds->buf.out_pos;
			enum xz_ret err = xz_dec_run(ds->xz, &ds->buf);
			size_t produced = ds->buf.out_pos - start_out;

			if (err == XZ_STREAM_END) {
				ds->finished = 1;
				break;
			} else if (err) {
				goto err;
			} else if (produced) {
				continue;
			}
		}

		// we need to get more data
		ds->buf.in = ds->source->read(ds->source, ds->buf.in_pos, ds->buf.in_size + 1, &ds->buf.in_size);
		ds->buf.in_pos = 0;
		if (!ds->buf.in_size) {
			break;
		}

	} while (ds->buf.out_pos < need);

	*plen = ds->buf.out_pos;
	return ds->buf.out;
err:
	*plen = 0;
	return NULL;
}

stream *open_xz_decoder(stream *source) {
	if (!source) {
		return NULL;
	}
	xz_crc32_init();
	xz_crc64_init();
	xz_stream *ds = calloc(1, sizeof(struct xz_stream));
	if (!ds) {
		source->close(source);
		return NULL;
	}
	struct xz_dec *xz = xz_dec_init(XZ_DYNALLOC, 100 * 1024 * 1024);
	if (!xz) {
		source->close(source);
		free(ds);
		return NULL;
	}
	ds->xz = xz;
	ds->source = source;
	ds->iface.close = &close_xz;
	ds->iface.read = &read_xz;
	return &ds->iface;
}
