#include "stream.h"
#include "zip.h"

static uint64_t ftell64(FILE *f) {
#ifdef _MSC_VER
	return _ftelli64(f);
#else
	return ftello(f);
#endif
}

static int fseek64(FILE *f, uint64_t off) {
#ifdef _MSC_VER
	return _fseeki64(f, off, SEEK_SET);
#else
	return fseeko(f, off, SEEK_SET);
#endif
}

static inline uint16_t little_16(const uint8_t *p) {
	return ((uint16_t) p[0])
		| ((uint16_t) p[1] << 8);
}
static inline uint32_t little_32(const uint8_t *p) {
	return ((uint32_t) p[0])
		| ((uint32_t) p[1] << 8)
		| ((uint32_t) p[2] << 16)
		| ((uint32_t) p[3] << 24);
}
static inline uint64_t little_64(const uint8_t *p) {
	return ((uint64_t) p[0])
		| ((uint64_t) p[1] << 8)
		| ((uint64_t) p[2] << 16)
		| ((uint64_t) p[3] << 24)
		| ((uint64_t) p[4] << 32)
		| ((uint64_t) p[5] << 40)
		| ((uint64_t) p[6] << 48)
		| ((uint64_t) p[7] << 56);
}

static void replace_char(char *p, char from, char to) {
	while (*p) {
		if (*p == from) {
			*p = to;
		}
		p++;
	}
}

static int64_t find_central_dir_64(FILE *f, int off) {
	zip64_central_dir_locator cd;
	if (fseek(f, -off - sizeof(cd), SEEK_END) || fread(&cd, 1, sizeof(cd), f) != sizeof(cd)) {
		fprintf(stderr, "failed to read footer in zip\n");
		return -2;
	}
	if (little_32(cd.sig) != ZIP64_CENTRAL_DIR_LOCATOR_SIG) {
		fprintf(stderr, "failed to find zip64 footer\n");
		return -2;
	}
	int64_t diroff = (int64_t) little_64(cd.dir_offset);
	if (diroff < 0) {
		fprintf(stderr, "invalid directory offset\n");
		return -2;
	}
	return diroff;
}

// returns +ve - found, -1 - not found, -2 - invalid file
static int64_t find_central_dir(FILE *f, int off) {
	char buf[65 * 1024];
	if (off < sizeof(zip_central_dir_end)) {
		fprintf(stderr, "invalid zip file\n");
		return -2;
	}
	if (fseek(f, -off, SEEK_END) || (int)fread(buf, 1, off, f) != off) {
		fprintf(stderr, "failed to read footer in zip\n");
		return -2;
	}

	for (int p = off - sizeof(zip_central_dir_end); p >= 0; p--) {
		zip_central_dir_end *c = (zip_central_dir_end*) &buf[p];
		if (little_32(c->sig) == ZIP_CENTRAL_DIR_END_SIG
		&& p + sizeof(*c) + little_16(c->comment_len) == (size_t)off) {
			uint32_t dir_offset = little_32(c->dir_offset);
			if (dir_offset == 0xFFFFFFFF) {
				return find_central_dir_64(f, p);
			} else {
				return dir_offset;
			}
		}
	}

	return -1;
}

struct zip_container {
	container h;
	FILE *f;
	uint64_t diroff;
	uint64_t header_off;
	uint64_t uncompressed_len;
	uint64_t compressed_len;
	uint16_t compression_method;
	char *path;
	unsigned executable : 1;
};

static int read_zip_file_header(struct zip_container *z) {
	zip_file_header fh;
	if (fread(&fh, 1, sizeof(fh), z->f) != sizeof(fh)
	|| little_32(fh.sig) != ZIP_FILE_HEADER_SIG) {
		// read all the files
		return 1;
	}

	z->header_off = little_32(fh.header_off);
	z->compressed_len = little_32(fh.compressed_len);
	z->uncompressed_len = little_32(fh.uncompressed_len);
	z->compression_method = little_16(fh.compression_method);
	z->executable = (little_32(fh.external_attributes) & (0100 << 16)) != 0;
	
	uint16_t namelen = little_16(fh.name_len);

	char *name = (char*) malloc(namelen + 1);
	if (fread(name, 1, namelen, z->f) != namelen) {
		free(name);
		return -1;
	}
	name[namelen] = 0;
	replace_char(name, '\\', '/');
	z->path = clean_path(name, "");
	free(name);
	if (!z->path) {
		fprintf(stderr, "invalid zip path %s\n", name);
		return -1;
	}

	int extralen = little_16(fh.extra_len);

	while (extralen >= 4) {
		zip_file_extra extra;
		if (fread(&extra, 1, sizeof(extra), z->f) != sizeof(extra)) {
			return -1;
		}
		uint16_t tag = little_16(extra.tag);
		uint16_t sz = little_16(extra.size);
		if (4 + sz > extralen) {
			break;
		}
		extralen -= 4 + sz;
		if (tag == ZIP64_FILE_EXTRA) {
			uint8_t buf[8];
			// only extract the 64 bit lengths if they are required
			if (z->uncompressed_len == 0xFFFFFFFF && sz >= 8) {
				if (fread(buf, 1, sizeof(buf), z->f) != sizeof(buf)) {
					return -1;
				}
				z->uncompressed_len = little_64(buf);
				sz -= 8;
			}
			if (z->compressed_len == 0xFFFFFFFF && sz >= 8) {
				if (fread(buf, 1, sizeof(buf), z->f) != sizeof(buf)) {
					return -1;
				}
				z->compressed_len = little_64(buf);
				sz -= 8;
			}
			if (z->header_off == 0xFFFFFFFF && sz >= 8) {
				if (fread(buf, 1, sizeof(buf), z->f) != sizeof(buf)) {
					return -1;
				}
				z->header_off = little_64(buf);
				sz -= 8;
			}
		}
		if (fseek(z->f, sz, SEEK_CUR)) {
			return -1;
		}
	}
	if (fseek(z->f, extralen, SEEK_CUR)) {
		return -1;
	}
	return 0;
}

static void close_zip(container *c) {
	struct zip_container *z = (struct zip_container*) c;
	free(z->path);
	free(z);
}

static stream *open_zip_file(container *c) {
	struct zip_container *z = (struct zip_container*) c;

	zip_local_header lh;
	if (fseek64(z->f, z->header_off)
		|| fread(&lh, 1, sizeof(lh), z->f) != sizeof(lh)
		|| fseek(z->f, little_16(lh.name_len) + little_16(lh.extra_len), SEEK_CUR)) {
		fprintf(stderr, "failed to read zip file header\n");
		return NULL;
	}

	stream *s = open_limited(open_file_stream(z->f), z->compressed_len);
	stream *zs = NULL;

	switch (z->compression_method) {
	case 8: // zlib - deflate
		zs = open_inflate(s);
		if (!zs) {
			s->close(s);
			return NULL;
		}
		return zs;
	case 0: // stored - no compression
		return s;
	default:
		fprintf(stderr, "unknown compression method %d\n", z->compression_method);
		s->close(s);
		return NULL;
	}
}

static int next_zip_file(container *c) {
	struct zip_container *z = (struct zip_container*) c;

	for (;;) {
		if (fseek64(z->f, z->diroff)) {
			fprintf(stderr, "failed to read file header\n");
			return -1;
		}
		int error = read_zip_file_header(z);
		if (error < 0) {
			return -1;
		} else if (error > 0) {
			// reached the end of the directory
			return 1;
		}

		// save off where we got to in the directory, for the next loop
		z->diroff = ftell64(z->f);

		// looking for files not directories
		if (z->path[strlen(z->path) - 1] == '/') {
			continue;
		}

		z->h.file_path = z->path;
		z->h.link_target = NULL;
		z->h.file_size = z->uncompressed_len;
		z->h.file_mode = z->executable ? 0755 : 0644;

		return 0;
	}
}

container *open_zip(FILE *f) {
	// find the central directory
	fseek(f, 0, SEEK_END);
	uint64_t zip_size = ftell64(f);

	int64_t diroff;
	int off = sizeof(zip_central_dir_end);
	for (;;) {
		if ((uint64_t)off > zip_size) {
			off = (int)zip_size;
		}

		diroff = find_central_dir(f, off);
		if (diroff < -1) {
			return NULL;
		} else if (diroff >= 0) {
			break;
		}
		switch (off) {
		case sizeof(zip_central_dir_end) :
			off = 1024;
			break;
		case 1024:
			off = 65 * 1024;
			break;
		default:
			fprintf(stderr, "failed to find central directory\n");
			return NULL;
		}
	}

	struct zip_container *z = calloc(1, sizeof(*z));
	z->h.close = &close_zip;
	z->h.next_file = &next_zip_file;
	z->h.open_file = &open_zip_file;
	z->f = f;
	z->diroff = diroff;
	return &z->h;
}
