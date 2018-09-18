#include "stream.h"
#include "zlib/zlib.h"
#include <string.h>

typedef struct inflate_stream inflate_stream;

struct inflate_stream {
	stream iface;
	z_stream z;
	size_t avail, bufsz, consumed;
	int finished;
	stream *source;
	uint8_t *buf;
	unsigned read_in;
};

static void close_inflate(stream *s) {
	inflate_stream *ds = (inflate_stream*)s;
	ds->source->close(ds->source);
	inflateEnd(&ds->z);
	free(ds->buf);
	free(ds);
}

static const uint8_t *read_inflate(stream *s, size_t consume, size_t need, size_t *plen) {
	inflate_stream *ds = (inflate_stream*) s;

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
		if (ds->z.avail_in) {
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

			int res = inflate(&ds->z, 0);
			size_t produced = ds->bufsz - ds->avail - ds->z.avail_out;
			ds->avail += produced;

			if (res == Z_STREAM_END) {
				ds->finished = 1;
				break;
			} else if (res) {
				fprintf(stderr, "zlib inflate error %d\n", res);
				goto err;
			} else if (produced) {
				continue;
			}
		}

		// read more from upstream
		size_t avail_in;
		ds->z.next_in = (uint8_t*) ds->source->read(ds->source, ds->read_in - ds->z.avail_in, ds->z.avail_in + 1, &avail_in);
		ds->z.avail_in = (avail_in > UINT_MAX) ? UINT_MAX : (unsigned)avail_in;
		ds->read_in = ds->z.avail_in;
		if (!ds->read_in) {
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

static stream *open_zlib(stream *source, int window) {
	if (!source) {
		return NULL;
	}
	inflate_stream *ds = calloc(1, sizeof(struct inflate_stream));
	if (!ds) {
		source->close(source);
		return NULL;
	}
	if (inflateInit2(&ds->z, window)) {
		source->close(source);
		free(ds);
		return NULL;
	}
	ds->source = source;
	ds->iface.close = &close_inflate;
	ds->iface.read = &read_inflate;
	return &ds->iface;

}

stream *open_inflate(stream *source) {
	return open_zlib(source, -15);
}

stream *open_gzip(stream *source) {
	return open_zlib(source, 15 + 32);
}
