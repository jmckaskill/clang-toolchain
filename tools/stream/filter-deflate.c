#include "stream.h"
#include "zlib/zlib.h"
#include <string.h>

typedef struct inflate_stream inflate_stream;

struct inflate_stream {
	stream iface;
	z_stream z;
	uint8_t buf[256*1024];
	size_t consumed, avail;
	int finished;
	stream *source;
};

static void close_inflate(stream *s) {
	inflate_stream *ds = (inflate_stream*)s;
	ds->source->close(ds->source);
	inflateEnd(&ds->z);
	free(ds);
}

static const uint8_t *inflate_data(stream *s, size_t *plen, int *atend) {
	inflate_stream *ds = (inflate_stream*) s;
	*plen = ds->avail - ds->consumed;
	*atend = ds->finished;
	return ds->buf + ds->consumed;
}

static void consume_inflate(stream *s, size_t consume) {
	inflate_stream *ds = (inflate_stream*) s;
	ds->consumed += consume;
}

static int inflate_more(stream *s) {
	inflate_stream *ds = (inflate_stream*) s;
	if (ds->finished) {
		fprintf(stderr, "calling inflate more after finished\n");
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
			ds->z.avail_in = len > UINT_MAX ? UINT_MAX : (unsigned) len;
			ds->z.next_in = (uint8_t*) src;
			ds->z.next_out = ds->buf + ds->avail;
			ds->z.avail_out = (unsigned) (sizeof(ds->buf) - ds->avail);

			int res = inflate(&ds->z, 0);
			size_t consumed = len - ds->z.avail_in;
			size_t produced = sizeof(ds->buf) - ds->avail - ds->z.avail_out;

			ds->source->consume(ds->source, consumed);
			ds->avail += produced;

			if (res == Z_STREAM_END) {
				// check that upstream is done too
				ds->source->buffered(ds->source, &len, &atend);
				if (len || !atend) {
					fprintf(stderr, "zlib stream finished before source\n");
					return -1;
				}
				ds->finished = 1;
				return 0;
			} else if (res) {
				fprintf(stderr, "zlib inflate error %d\n", res);
				ds->finished = 1;
				return -1;
			} else if (produced) {
				// we made forward progress
				return 0;
			}
		}

		// ask upstream to get more
		if (ds->source->get_more(ds->source)) {
			return -1;
		}
	}
}

static stream *open_zlib(stream *source, int window) {
	inflate_stream *ds = malloc(sizeof(struct inflate_stream));
	if (!ds) {
		return NULL;
	}
	memset(&ds->z, 0, sizeof(ds->z));
	if (inflateInit2(&ds->z, window)) {
		free(ds);
		return NULL;
	}
	ds->source = source;
	ds->consumed = 0;
	ds->avail = 0;
	ds->finished = 0;
	ds->iface.close = &close_inflate;
	ds->iface.get_more = &inflate_more;
	ds->iface.buffered = &inflate_data;
	ds->iface.consume = &consume_inflate;
	return &ds->iface;

}

stream *open_inflate(stream *source) {
	return open_zlib(source, -15);
}

stream *open_gzip(stream *source) {
	return open_zlib(source, 15 + 32);
}
