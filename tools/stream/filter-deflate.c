#include "stream.h"
#include "zlib/zlib.h"
#include <string.h>
#include <stdbool.h>

typedef struct zlib_stream zlib_stream;

struct zlib_stream {
	stream iface;
	z_stream z;
	size_t avail, bufsz, consumed;
	int finished;
	stream *source;
	uint8_t *buf;
	unsigned read_in;
	int flush;
	bool do_inflate;
};

static void close_zlib(stream *s) {
	zlib_stream *ds = (zlib_stream*)s;
	ds->source->close(ds->source);
	inflateEnd(&ds->z);
	free(ds->buf);
	free(ds);
}

static const uint8_t *read_zlib(stream *s, size_t consume, size_t need, size_t *plen) {
	zlib_stream *ds = (zlib_stream*) s;

	// see if we can service from the existing buffer
	ds->consumed += consume;
	if (ds->finished || ds->consumed + need <= ds->avail) {
		*plen = ds->avail - ds->consumed;
		return ds->buf + ds->consumed;
	}

	// compress the buffer
	if (ds->consumed && ds->consumed < ds->avail) {
		memmove(ds->buf, ds->buf + ds->consumed, ds->avail - ds->consumed);
	}
	ds->avail -= ds->consumed;
	ds->consumed = 0;

	// get more data
	do {
		if (ds->z.avail_in || ds->flush) {
			if (ds->avail == ds->bufsz) {
				size_t bufsz = ds->bufsz + 32 * 1024;
				uint8_t *buf = realloc(ds->buf, bufsz);
				if (!buf) {
					goto err;
				}
				ds->bufsz = bufsz;
				ds->buf = buf;
			}

			ds->z.next_out = ds->buf + ds->avail;
			ds->z.avail_out = (unsigned) (ds->bufsz - ds->avail);

			int res = ds->do_inflate ? inflate(&ds->z, ds->flush) : deflate(&ds->z, ds->flush);
			size_t produced = ds->bufsz - ds->avail - ds->z.avail_out;
			ds->avail += produced;

			if (res == Z_STREAM_END) {
				ds->finished = 1;
				break;
			} else if (res) {
				fprintf(stderr, "zlib error %d\n", res);
				goto err;
			} else if (produced) {
				continue;
			}
		}

		// read more from upstream
		if (!ds->flush) {
			size_t avail_in;
			ds->z.next_in = (uint8_t*)ds->source->read(ds->source, ds->read_in - ds->z.avail_in, ds->z.avail_in + 1, &avail_in);
			ds->z.avail_in = (avail_in > UINT_MAX) ? UINT_MAX : (unsigned)avail_in;
			ds->read_in = ds->z.avail_in;
			if (!ds->read_in) {
				ds->flush = Z_FINISH;
			}
		} else {
			break;
		}

	} while (need > ds->avail);

	*plen = ds->avail;
	return ds->buf;
err:
	ds->finished = 1;
	*plen = 0;
	return NULL;
}

static stream *open_zlib(stream *source, bool do_inflate, int window) {
	if (!source) {
		return NULL;
	}
	zlib_stream *ds = calloc(1, sizeof(struct zlib_stream));
	if (!ds) {
		source->close(source);
		return NULL;
	}
	if (do_inflate ? inflateInit2(&ds->z, window) : deflateInit2(&ds->z, Z_BEST_COMPRESSION, Z_DEFLATED, window, 9, Z_DEFAULT_STRATEGY)) {
		source->close(source);
		free(ds);
		return NULL;
	}
	ds->do_inflate = do_inflate;
	ds->source = source;
	ds->iface.close = &close_zlib;
	ds->iface.read = &read_zlib;
	return &ds->iface;
}

stream *open_deflate(stream *source) {
	return open_zlib(source, false, -15);
}

stream *open_inflate(stream *source) {
	return open_zlib(source, true, -15);
}

stream *open_gzip(stream *source) {
	return open_zlib(source, true, 15 + 32);
}
