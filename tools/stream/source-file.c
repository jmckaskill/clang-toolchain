#include "stream.h"
#include <stdio.h>
#include <string.h>

typedef struct file_stream file_stream;

struct file_stream {
	stream iface;
	FILE *f;
	size_t consumed, avail;
	uint8_t buf[256*1024];
};

static uint8_t *file_data(stream *s, size_t *plen, size_t *atend) {
	file_stream *fs = (file_stream*) s;
	*plen = fs->avail - fs->consumed;
	*atend = (fs->f == NULL);
	return fs->buf + fs->consumed;
}

static void consume_file(stream *s, size_t consumed) {
	file_stream *fs = (file_stream*) s;
	fs->consumed += consumed;
}

static int read_more(stream *s) {
	file_stream *fs = (file_stream*) s;
	if (!fs->f) {
		fprintf(stderr, "calling read more after the read is finished\n");
		return -1;
	}
	if (fs->consumed && fs->consumed < fs->avail) {
		memmove(fs->buf, fs->buf + fs->consumed, fs->avail - fs->consumed);
	}
	fs->avail -= fs->consumed;
	fs->consumed = 0;

	fs->avail += fread(fs->buf + fs->avail, 1, sizeof(fs->buf) - fs->avail, fs->f);

	if (ferror(fs->f)) {
		fprintf(stderr, "error on read\n");
		fs->f = NULL;
		return -1;
	} else if (feof(fs->f)) {
		fs->f = NULL;
	}

	return 0;
}

static file_stream gfs;

stream *open_file(FILE *f) {
	gfs.f = f;
	gfs.consumed = 0;
	gfs.avail = 0;
	gfs.iface.get_more = &read_more;
	gfs.iface.buffered = &file_data;
	gfs.iface.consume = &consume_file;
	return &gfs.iface;
}

