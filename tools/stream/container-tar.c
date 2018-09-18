#include "stream.h"
#include "tar.h"

#ifndef WIN32
#include <unistd.h>
#endif

static const char *strip_top_dir(const char *name) {
	const char *slash = strchr(name, '/');
	return slash ? (slash + 1) : (name + strlen(name));
}

static char *clean_tar_name(const tar_posix_header *t) {
	// strip the first directory component for tar
	char name[sizeof(t->name)+1];
	strncpy(name, t->name, sizeof(t->name));
	name[sizeof(t->name)] = 0;
	if (!memcmp(t->magic, "ustar\0", 6) && t->prefix[0]) {
		char prefix[sizeof(t->prefix) + 1];
		strncpy(prefix, t->prefix, sizeof(t->prefix));
		prefix[sizeof(t->prefix)] = 0;
		return clean_path(strip_top_dir(prefix), name);
	} else {
		return clean_path("", strip_top_dir(name));
	}
}

static char *clean_tar_linkname(const tar_posix_header *t) {
	char *lnk = malloc(sizeof(t->linkname) + 1);
	strncpy(lnk, t->linkname, sizeof(t->linkname));
	lnk[sizeof(t->linkname)] = 0;
	if (!*lnk || strchr(lnk, ':') || strchr(lnk, '/') || strchr(lnk, '\\') || !strcmp(lnk, ".") || !strcmp(lnk, "..")) {
		free(lnk);
		return NULL;
	}
	return lnk;
}

#define TAR_BLOCK_SIZE UINT64_C(512)

struct tar_container {
	container h;
	stream *s;
	char *path;
	char *lnk;
	size_t padding;
	unsigned file_opened : 1;
};

static void close_tar(container *iface) {
	struct tar_container *c = (struct tar_container*) iface;
	c->s->close(c->s);
	free(c->path);
	free(c->lnk);
	free(c);
}

static int read_tar_file_header(struct tar_container *c, const tar_posix_header *t) {
	char mode[sizeof(t->mode) + 1];
	strncpy(mode, t->mode, sizeof(t->mode));
	mode[sizeof(t->mode)] = 0;
	int imode = strtol(mode, NULL, 8);

	char *path = clean_tar_name(t);
	if (!path) {
		fprintf(stderr, "invalid tar path %s\n", t->name);
		return -1;
	}

	char size[sizeof(t->size) + 1];
	strncpy(size, t->size, sizeof(t->size));
	size[sizeof(t->size)] = 0;

	uint64_t sz = strtoull(size, NULL, 8);

	c->file_opened = 0;
	c->path = path;
	c->lnk = NULL;
	// tar files are padded out to the block size
	c->padding = (size_t) (((sz + TAR_BLOCK_SIZE - 1) &~(TAR_BLOCK_SIZE - 1)) - sz);
	c->h.file_path = path;
	c->h.link_target = NULL;
	c->h.file_size = sz;
	c->h.file_mode = imode;
	return 0;
}

static int read_tar_link_header(struct tar_container *c, const tar_posix_header *t) {
	char *path = clean_tar_name(t);
	if (!path) {
		fprintf(stderr, "invalid link path %s\n", t->name);
		return -1;
	}
	char *lnk = clean_tar_linkname(t);
	if (!lnk) {
		free(path);
		fprintf(stderr, "invalid link target %s\n", t->linkname);
		return -1;
	}
	c->file_opened = 1;
	c->path = path;
	c->lnk = lnk;
	c->padding = 0;
	c->h.file_path = path;
	c->h.link_target = lnk;
	c->h.file_size = 0;
	c->h.file_mode = 0;
	return 0;
}

static int next_tar_file(container *iface) {
	struct tar_container *c = (struct tar_container*) iface;

	free(c->path);
	free(c->lnk);
	c->path = NULL;
	c->lnk = NULL;

	uint64_t skip = c->padding;
	if (!c->file_opened) {
		skip += c->h.file_size;
	}
	c->padding = 0;

	size_t consume = 0;
	while (skip) {
		c->s->read(c->s, consume, 1, &consume);
		if (!consume) {
			fprintf(stderr, "early close of stream\n");
			return -1;
		}
		if ((uint64_t) consume > skip) {
			consume = (size_t)skip;
		}
		skip -= consume;
	}

	for (;;) {
		size_t len;
		const uint8_t *p = c->s->read(c->s, consume, TAR_BLOCK_SIZE, &len);
		if (!len) {
			// end of file
			return 1;
		} else if (len < TAR_BLOCK_SIZE) {
			// error
			return -1;
		}

		const tar_posix_header *t = (const tar_posix_header*)p;
		int err;

		switch (t->typeflag) {
		case LNKTYPE:
		case SYMTYPE:
			err = read_tar_link_header(c, t);
			c->s->read(c->s, TAR_BLOCK_SIZE, 0, &consume);
			return err;
		case REGTYPE:
			err = read_tar_file_header(c, t);
			c->s->read(c->s, TAR_BLOCK_SIZE, 0, &consume);
			return err;
		default:
			consume = TAR_BLOCK_SIZE;
			continue;
		}
	}
}

static stream *open_tar_file(container *iface) {
	struct tar_container *c = (struct tar_container*) iface;
	if (c->file_opened) {
		return NULL;
	}
	c->file_opened = 1;
	return open_limited(c->s, c->h.file_size);
}

container *open_tar(stream *s) {
	struct tar_container *c = calloc(1, sizeof(*c));
	c->h.close = &close_tar;
	c->h.next_file = &next_tar_file;
	c->h.open_file = &open_tar_file;
	c->s = s;
	return &c->h;
}
