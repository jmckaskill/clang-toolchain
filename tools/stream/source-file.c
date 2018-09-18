#include "stream.h"
#include <stdio.h>
#include <string.h>

typedef struct file_stream file_stream;

struct file_stream {
	stream iface;
	FILE *f;
	size_t avail, bufsz, consumed;
	uint8_t *buf;
};

static void close_file(stream *s) {
	file_stream *fs = (file_stream*)s;
	free(fs->buf);
	free(fs);
}

static const uint8_t *read_file(stream *s, size_t consume, size_t need, size_t *plen) {
	file_stream *fs = (file_stream*) s;

	// see if we can service the request from the existing buffer
	fs->consumed += consume;
	if (!fs->f || fs->consumed + need <= fs->avail) {
		*plen = fs->avail - fs->consumed;
		return fs->buf + fs->consumed;
	}

	// compress the buffer
	if (fs->consumed && fs->consumed < fs->avail) {
		memmove(fs->buf, fs->buf + fs->consumed, fs->avail - fs->consumed);
	}
	fs->avail -= fs->consumed;
	fs->consumed = 0;

	do {
		if (fs->avail == fs->bufsz) {
			size_t bufsz = fs->bufsz + 32 * 1024;
			uint8_t *buf = realloc(fs->buf, bufsz);
			if (!buf) {
				goto err;
			}
			fs->buf = buf;
			fs->bufsz = bufsz;
		}

		fs->avail += fread(fs->buf + fs->avail, 1, fs->bufsz - fs->avail, fs->f);

		if (ferror(fs->f)) {
			goto err;
		} else if (feof(fs->f)) {
			fs->f = NULL;
			break;
		}
	} while (fs->avail < need);

	*plen = fs->avail;
	return fs->buf;
err:
	fs->f = NULL;
	*plen = 0;
	return NULL;
}

stream *open_file_stream(FILE *f) {
	file_stream *fs = calloc(1, sizeof(struct file_stream));
	if (!fs) {
		return NULL;
	}
	fs->f = f;
	fs->iface.close = &close_file;
	fs->iface.read = &read_file;
	return &fs->iface;
}

