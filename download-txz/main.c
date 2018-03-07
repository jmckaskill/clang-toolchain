#include "download-txz.h"
#include "tar.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(x,mode) _mkdir(x)
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#define BLOCK_SIZE 512

static uint8_t *get_block(struct decode_stream *ds, struct download_stream *os) {
	for (;;) {
		int len;
		uint8_t *p = decoded_data(ds, &len);
		if (len >= BLOCK_SIZE) {
			return p;
		}

		// not enough already decoded, so see if we can decode more
		p = downloaded_data(os, &len);
		if (!len) {
			// need to download some more first
			if (download_more(os)) {
				return NULL;
			}
			p = downloaded_data(os, &len);
		}

		int used = decode_more(ds, p, len);
		if (used < 0) {
			return NULL;
		}
		download_used(os, used);
	}
}
int main() {
	struct download_stream *os = open_download_stream("storage.googleapis.com", "/ctct-clang-toolchain/libarm-2018-03-02-2.txz");
	struct decode_stream *ds = open_decode_stream();
	if (!os || !ds) {
		return 1;
	}

	for (;;) {
		uint8_t *p = get_block(ds, os);
		if (!p) {
			break;
		}

		struct posix_header *t = (struct posix_header*) p;
		t->name[sizeof(t->name) - 1] = 0;
		fprintf(stderr, "%s\n", t->name);
		t->size[sizeof(t->size) - 1] = 0;
		uint64_t sz = strtoull(t->size, NULL, 8);

		decode_used(ds, BLOCK_SIZE);

		FILE *f = NULL;
		switch (t->typeflag) {
		case REGTYPE:
			f = fopen(t->name, "wb");
			if (f == NULL) {
				fprintf(stderr, "failed to open %s\n", t->name);
			}
			break;
		case DIRTYPE:
			if (mkdir(t->name, 755)) {
				fprintf(stderr, "failed to creat %s\n", t->name);
			}
			break;
		}

		while (sz) {
			p = get_block(ds, os);
			if (p == NULL) {
				return 3;
			}

			int touse = BLOCK_SIZE;
			if ((uint64_t) touse > sz) {
				touse = (int) sz;
			}
			if (f) {
				fwrite(p, 1, touse, f);
			}
			decode_used(ds, BLOCK_SIZE);
			sz -= touse;
		}

		if (f) {
			fclose(f);
		}
	}

	if (!download_finished(os) || !decode_finished(ds)) {
		fprintf(stderr, "mismatch in finish state\n");
		return 3;
	}

	return 0;
}

