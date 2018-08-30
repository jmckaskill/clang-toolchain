#include "stream.h"

#ifdef WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#endif

typedef struct buf_stream buf_stream;
typedef struct map_stream map_stream;

struct buf_stream {
	stream iface;
	uint8_t *next;
	size_t remaining;
};

static void close_buffer(stream *s) {
	free(s);
}

static const uint8_t *buf_data(stream *s, size_t *plen, int *atend) {
	buf_stream *bs = (buf_stream*)s;
	*plen = bs->remaining;
	*atend = bs->remaining == 0;
	return bs->next;
}

static void consume_buf(stream *s, size_t consumed) {
	buf_stream *bs = (buf_stream*)s;
	bs->next += consumed;
	bs->remaining -= consumed;
}

static int more_buf(stream *s) {
	return 0;
}

static stream *setup_buf_stream(buf_stream *bs, const void *data, size_t size) {
	bs->iface.buffered = &buf_data;
	bs->iface.consume = &consume_buf;
	bs->iface.get_more = &more_buf;
	bs->next = (uint8_t*)data;
	bs->remaining = size;
	return &bs->iface;
}

stream *open_buffer_stream(const void *data, size_t size) {
	buf_stream *bs = (buf_stream*)malloc(sizeof(buf_stream));
	if (!bs) {
		return NULL;
	}
	bs->iface.close = &close_buffer;
	return setup_buf_stream(bs, data, size);
}

#ifdef WIN32
struct map_stream {
	buf_stream bs;
	void *data;
	HANDLE h;
};
void close_mapped_file(stream *s) {
	map_stream *ms = (map_stream*) s;
	UnmapViewOfFile(ms->data);
	CloseHandle(ms->h);
	free(ms);
}
stream *open_mapped_file(const char *filename) {
	HANDLE fd = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fd == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	LARGE_INTEGER sz;
	if (!GetFileSizeEx(fd, &sz) || sz.QuadPart > (uint64_t)SIZE_MAX) {
		CloseHandle(fd);
		return NULL;
	}

	HANDLE md = CreateFileMapping(fd, NULL, PAGE_READONLY, sz.HighPart, sz.LowPart, NULL);
	if (md == INVALID_HANDLE_VALUE) {
		CloseHandle(fd);
		return NULL;
	}
	// we no longer need the file handle, the mapping handle will keep the view open
	CloseHandle(fd);

	void *ptr = MapViewOfFileEx(md, FILE_MAP_READ, 0, 0, (size_t)sz.QuadPart, NULL);
	if (!ptr) {
		CloseHandle(md);
		return NULL;
	}

	map_stream *ms = (map_stream*) malloc(sizeof(map_stream));
	if (!ms) {
		CloseHandle(md);
		return NULL;
	}
	ms->h = md;
	ms->data = ptr;
	ms->bs.iface.close = &close_mapped_file;
	return setup_buf_stream(&ms->bs, ptr, (size_t)sz.QuadPart);
}

#else
struct map_stream {
	buf_stream bs;
	void *data;
	size_t size;
	int fd;
};
void close_mapped_file(stream *s) {
	map_stream *ms = (map_stream*)s;
	munmap(ms->data, ms->size);
	close(ms->fd);
	free(ms);
}
stream *open_mapped_file(const char *filename) {
	int fd = open(filename, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		return NULL;
	}
	struct stat st;
	if (fstat(fd, &st)) {
		close(fd);
		return NULL;
	}

	if (st.st_size > SIZE_MAX) {
		close(fd);
		return NULL;
	}

	size_t sz = (size_t)st.st_size;
	void *p = mmap(NULL, sz, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
	if (p == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	map_stream *ms = (map_stream*)malloc(sizeof(map_stream));
	if (!ms) {
		close(fd);
		return NULL;
	}

	ms->fd = fd;
	ms->data = p;
	ms->size = sz;
	ms->bs.iface.close = &close_mapped_file;
	return setup_buf_stream(&ms->bs, p, sz);
}
#endif



