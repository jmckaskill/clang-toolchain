#include "stream.h"
#include "tar.h"

static const char *strip_top_dir(const char *name) {
	const char *slash = strchr(name, '/');
	return slash ? (slash + 1) : (name + strlen(name));
}

static char *clean_tar_name(const char *dir, tar_posix_header *t) {
	// strip the first directory component for tar
	t->mode[0] = 0;
	if (!memcmp(t->magic, "ustar\0", 6) && t->prefix[0]) {
		t->rest[0] = 0;
		return clean_path(dir, strip_top_dir(t->prefix), t->name);
	} else {
		return clean_path(dir, "", strip_top_dir(t->name));
	}
}

static const char *clean_tar_linkname(const char *dir, tar_posix_header *t) {
	t->magic[0] = 0;
	char *p = t->linkname;
	if (!*p || strchr(p, ':') || strchr(p, '/') || strchr(p, '\\') || !strcmp(p, ".") || !strcmp(p, "..")) {
		return NULL;
	}
	return p;
}

#define TAR_BLOCK_SIZE UINT64_C(512)

static int extract_tar_file(tar_posix_header *t, stream *s, const char *dir) {
#ifndef _WIN32
	// terminate the mode field
	t->uid[0] = 0;
	int mode = strtol(t->mode, NULL, 8);
#endif

	char *path = clean_tar_name(dir, t);
	if (!path) {
		fprintf(stderr, "invalid tar path %s\n", t->name);
		return -1;
	}
	printf("extracting %s\n", path);

	// terminate the size field
	t->mtime[0] = 0;
	uint64_t sz = strtoull(t->size, NULL, 8);
	s->consume(s, TAR_BLOCK_SIZE);
	// at this point we can no longer use t, as the underlying buffer has been removed
	t = NULL;

	if (extract_file(open_limited(s, sz), path, 0)) {
		return -1;
	}

#ifndef _WIN32
	chmod(path, mode);
#endif

	// tar files are padded out to the block size
	uint64_t extra = ((sz + TAR_BLOCK_SIZE - 1) &~ (TAR_BLOCK_SIZE-1)) - sz;
	while (extra) {
		int len, atend;
		s->buffered(s, &len, &atend);
		if ((uint64_t)len > extra) {
			len = (int)extra;
		}
		if (len) {
			s->consume(s, len);
			extra -= len;
		} else if (atend) {
			fprintf(stderr, "early close of stream\n");
			return -1;
		} else if (s->get_more(s)) {
			return -1;
		}
	}

	return 0;
}

static int extract_tar_link(tar_posix_header *t, const char *dir) {
#ifdef _WIN32
	return 0;
#else
	char *path = clean_tar_name(dir, t);
	if (!path) {
		fprintf(stderr, "invalid link path %s\n", t->name);
		return -1;
	}
	const char *lnk = clean_tar_linkname(dir, t);
	if (!lnk) {
		free(path);
		fprintf(stderr, "invalid link target %s\n", t->linkname);
		return -1;
	}
	printf("extracting %s -> %s\n", path, lnk);
	make_directory(path);
	int err = symlink(lnk, path);
	free(path);
	return err;
#endif
}

int extract_tar(stream *s, const char *dir) {
	for (;;) {
		int len, atend;
		uint8_t *p = s->buffered(s, &len, &atend);
		if (len >= TAR_BLOCK_SIZE) {
			tar_posix_header *t = (tar_posix_header*)p;
			int err;

			switch (t->typeflag) {
			case LNKTYPE:
			case SYMTYPE:
				err = extract_tar_link(t, dir);
				s->consume(s, TAR_BLOCK_SIZE);
				break;
			case REGTYPE:
				err = extract_tar_file(t, s, dir);
				break;
			default:
				err = 0;
				s->consume(s, TAR_BLOCK_SIZE);
				break;
			}

			if (err) {
				return err;
			}

		} else if (len && atend) {
			fprintf(stderr, "premature end of tar file\n");
			return -1;
		} else if (atend) {
			// end of tar file
			return 0;
		} else if (s->get_more(s)) {
			return -1;
		}
	}
}
