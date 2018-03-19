#include "stream.h"
#include "tar.h"
#include "zip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <shellapi.h>
#else
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

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

#define TAR_BLOCK_SIZE UINT64_C(512)

static uint64_t get_file_size(FILE *f) {
	fseek(f, 0, SEEK_END);
	uint64_t sz = ftell64(f);
	fseek(f, 0, SEEK_SET);
	return sz;
}

static void make_directory(char *file) {
	char *next = file;
	for (;;) {
		char *slash = strchr(next, '/');
		if (!slash) {
			return;
		}
		*slash = 0;
		#ifdef _WIN32
		_mkdir(file);
		#else
		mkdir(file, 0755);
		#endif
		*slash = '/';
		next = slash + 1;
	}
}

static int extract_file(char *path, stream *s) {
	make_directory(path);
	fprintf(stderr, "extracting %s\n", path);

	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "failed to open %s\n", path);
		return -1;
	}

	uint64_t total = 0;
	for (;;) {
		int len, atend;
		uint8_t *p = s->buffered(s, &len, &atend);
		if (len) {
			total += len;
			fprintf(stderr, "downloaded %"PRIu64 "\n", total);
			if ((int) fwrite(p, 1, len, f) != len) {
				fprintf(stderr, "failed to write output file\n");
				fclose(f);
				return -1;
			}
			s->consume(s, len);
		} else if (atend) {
			fclose(f);
			return 0;
		} else if (s->get_more(s)) {
			return -1;
		}
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

static char *clean_path(const char *dir, char *name) {
	for (char *p = name; *p; p++) {
		if (*p == '\\') {
			*p = '/';
		}
	}
	if (strchr(name, ':')) {
		return NULL;
	}
	if (*name == '/' || *name == '\0') {
		return NULL;
	}
	// verify the path isn't trying to escape
	const char *next = name;
	for (;;) {
		if (!strcmp(next, "..") || !strncmp(next, "../", 3)) {
			return NULL;
		}
		const char *slash = strchr(next, '/');
		if (!slash) {
			break;
		}
		next = slash + 1;
	}

	char *ret = (char*) malloc(strlen(dir) + 1 + strlen(name) + 1);
	strcpy(ret, dir);
	strcat(ret, "/");
	strcat(ret, name);
	return ret;
}

static char *clean_tar_path(const char *dir, char *name) {
	// strip the first directory component for tar
	char *slash = strchr(name, '/');
	return clean_path(dir, slash ? (slash + 1) : (name + strlen(name)));
}

typedef struct zip_file zip_file;

struct zip_file {
	uint64_t header_off;
	uint64_t uncompressed_len;
	uint64_t compressed_len;
	uint16_t compression_method;
	char *path;
};

static int extract_zip_file(FILE *f, const zip_file *zf) {
	zip_local_header lh;
	if (fseek64(f, zf->header_off)
	|| fread(&lh, 1, sizeof(lh), f) != sizeof(lh)
	|| fseek(f, little_16(lh.name_len) + little_16(lh.extra_len), SEEK_CUR)) {
		fprintf(stderr, "failed to read zip file header\n");
		return -1;
	}

	stream *s = open_limited(open_file_source(f), zf->compressed_len);

	switch (zf->compression_method) {
	case 8: // zlib - deflate
		if ((s = open_inflate(s)) == NULL) {
			return -1;
		}
		break;
	case 0: // stored - no compression
		break;
	}

	return extract_file(zf->path, s);
}

static int read_zip_file_header(FILE *f, const char *dir, zip_file *zf) {
	zip_file_header fh;
	if (fread(&fh, 1, sizeof(fh), f) != sizeof(fh)
	|| little_32(fh.sig) != ZIP_FILE_HEADER_SIG) {
		// read all the files
		return 1;
	}

	zf->header_off = little_32(fh.header_off);
	zf->compressed_len = little_32(fh.compressed_len);
	zf->uncompressed_len = little_32(fh.uncompressed_len);
	zf->compression_method = little_16(fh.compression_method);

	uint16_t namelen = little_16(fh.name_len);

	char *name = (char*) malloc(namelen + 1);
	if (fread(name, 1, namelen, f) != namelen) {
		free(name);
		return -1;
	}
	name[namelen] = 0;
	zf->path = clean_path(dir, name);
	free(name);
	if (!zf->path) {
		fprintf(stderr, "invalid zip path %s\n", name);
		return -1;
	}

	int extralen = little_16(fh.extra_len);

	while (extralen >= 4) {
		zip_file_extra extra;
		if (fread(&extra, 1, sizeof(extra), f) != sizeof(extra)) {
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
			if (zf->uncompressed_len == 0xFFFFFFFF && sz >= 8) {
				if (fread(buf, 1, sizeof(buf), f) != sizeof(buf)) {
					return -1;
				}
				zf->uncompressed_len = little_64(buf);
				sz -= 8;
			}
			if (zf->compressed_len == 0xFFFFFFFF && sz >= 8) {
				if (fread(buf, 1, sizeof(buf), f) != sizeof(buf)) {
					return -1;
				}
				zf->compressed_len = little_64(buf);
				sz -= 8;
			}
			if (zf->header_off == 0xFFFFFFFF && sz >= 8) {
				if (fread(buf, 1, sizeof(buf), f) != sizeof(buf)) {
					return -1;
				}
				zf->header_off = little_64(buf);
				sz -= 8;
			}
		}
		if (fseek(f, sz, SEEK_CUR)) {
			return -1;
		}
	}
	if (fseek(f, extralen, SEEK_CUR)) {
		return -1;
	}
	return 0;
}

static int extract_zip(const char *path, stream *s) {
	if (extract_file("download.tmp.zip", s)) {
		return -1;
	}

	fprintf(stderr, "extracting zip into %s\n", path);

	FILE *f = fopen("download.tmp.zip", "rb");
	if (!f) {
		fprintf(stderr, "failed to open temp zip file\n");
		goto err;
	}

	int64_t diroff;
	uint64_t zip_size = get_file_size(f);
	int off = sizeof(zip_central_dir_end);
	for (;;) {
		if ((uint64_t) off > zip_size) {
			off = (int) zip_size;
		}

		diroff = find_central_dir(f, off);
		if (diroff < -1) {
			goto err;
		} else if (diroff >= 0) {
			break;
		}
		switch (off) {
		case sizeof(zip_central_dir_end):
			off = 1024;
			break;
		case 1024:
			off = 65 * 1024;
			break;
		default:
			fprintf(stderr, "failed to find central directory\n");
			goto err;
		}
	}

	for (;;) {
		if (fseek64(f, diroff)) {
			fprintf(stderr, "failed to read file header\n");
			goto err;
		}
		zip_file zf;
		int error = read_zip_file_header(f, path, &zf);
		if (error < 0) {
			goto err;
		} else if (error > 0) {
			// reached the end of the directory
			break;
		}
		// save off where we got to in the directory, for the next loop
		diroff = ftell64(f);
		// extract off the file, this will reseek in the zip file
		if (extract_zip_file(f, &zf)) {
			goto err;
		}
	}
	fclose(f);
	unlink("download.tmp.zip");
	return 0;

err:
	fclose(f);
	return -1;
}

static int extract_tar_file(tar_posix_header *t, stream *s, const char *dir) {
	char *path = clean_tar_path(dir, t->name);
	if (!path) {
		fprintf(stderr, "invalid tar path %s\n", t->name);
		return -1;
	}

	uint64_t sz = strtoull(t->size, NULL, 8);
	s->consume(s, TAR_BLOCK_SIZE);
	// at this point we can no longer use t, as the underlying buffer has been removed
	t = NULL;

	if (extract_file(path, open_limited(s, sz))) {
		return -1;
	}

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
	char *lnk = clean_tar_path(dir, t->linkname);
	if (!lnk) {
		fprintf(stderr, "invalid link target %s\n", t->linkname);
		return -1;
	}
	char *path = clean_tar_path(dir, t->name);
	if (!path) {
		free(lnk);
		fprintf(stderr, "invalid link path %s\n", t->name);
		return -1;
	}
	int err = symlink(lnk, path);
	free(lnk);
	free(path);
	return err;
#endif
}

static int extract_tar(const char *dir, stream *s) {
	for (;;) {
		int len, atend;
		uint8_t *p = s->buffered(s, &len, &atend);
		if (len >= TAR_BLOCK_SIZE) {
			tar_posix_header *t = (tar_posix_header*)p;
			if (!memchr(t->name, 0, sizeof(t->name))
			|| !memchr(t->linkname, 0, sizeof(t->linkname))) {
				fprintf(stderr, "invalid tar file\n");
				return -1;
			}
			// terminate the size field
			t->mtime[0] = 0;
			// terminate the mode field
			t->uid[0] = 0;

			int err;

			switch (t->typeflag) {
			case LNKTYPE:
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

static char *trim(char *s) {
	while (*s == ' ' || *s == '\t') {
		s++;
	}
	char *e = s + strlen(s);
	while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) {
		e--;
	}
	*e = 0;
	return s;
}

static int is_host_platform(char *platform) {
	if (!platform) {
		return 1;
	}
	platform = trim(platform);
#if defined _WIN32
	return !strcmp(platform, "windows");
#elif defined __MACH__
	return !strcmp(platform, "mac");
#elif defined __linux__
	return !strcmp(platform, "linux");
#else
#error "unknown platform"
#endif
}

static int has_download_done(const char *findpath, const char *testurl) {
	FILE *fdone = fopen("download.done", "r");
	if (fdone) {
		char line[256];
		while (fgets(line, sizeof(line), fdone)) {
			char *path = trim(strtok(line, ","));
			char *url = trim(strtok(NULL, ","));
			if (path && url && !strcmp(path, findpath)) {
				fclose(fdone);
				return !strcmp(url, testurl);
			}
		}
		fclose(fdone);
	}
	return 0;
}

static int update_done(const char *addpath, const char *addurl) {
	FILE *tmpf = fopen("download.tmp", "w");
	if (tmpf == NULL) {
		fprintf(stderr, "failed to open temp output file\n");
		return -1;
	}
	FILE *fdone = fopen("download.done", "r");
	if (fdone) {
		char line[256];
		while (fgets(line, sizeof(line), fdone)) {
			char *path = trim(strtok(line, ","));
			char *url = trim(strtok(NULL, ","));
			if (path && url && strcmp(path, addpath)) {
				fprintf(tmpf, "%s,%s\n", path, url);
			}
		}
		fclose(fdone);
		unlink("download.done");
	}
	fprintf(tmpf, "%s,%s\n", addpath, addurl);
	fclose(tmpf);
	if (rename("download.tmp", "download.done")) {
		fprintf(stderr, "failed to rewrite temp output file to actual output file\n");
		return -1;
	}
	return 0;
}

#ifdef _WIN32
static void delete_path(const char *path) {
	int bufsz = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
	wchar_t *paths = (wchar_t*)malloc((bufsz + 1) * 2);
	MultiByteToWideChar(CP_UTF8, 0, path, -1, paths, bufsz);
	paths[bufsz] = 0;
	SHFILEOPSTRUCTW op = { 0 };
	op.wFunc = FO_DELETE;
	op.pFrom = paths;
	op.fFlags = FOF_NO_UI;
	SHFileOperationW(&op);
	free(paths);
}
#else
static void delete_dir(int dirfd) {
	DIR *dirp = fdopendir(dirfd);
	if (!dirp) {
		return;
	}
	struct dirent *d;
	while ((d = readdir(dirp)) != NULL) {
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, "..")) {
			continue;
		}
		if (d->d_type == DT_DIR) {
			int fd = openat(dirfd, d->d_name, O_RDONLY);
			if (fd >= 0) {
				delete_dir(fd);
				close(fd);
			}
			unlinkat(dirfd, d->d_name, AT_REMOVEDIR);
		} else {
			unlinkat(dirfd, d->d_name, 0);
		}
	}
	closedir(dirp);
}
static void delete_path(const char *path) {
	int fd = open(path, O_RDONLY | O_SYMLINK);
	if (fd < 0) {
		return;
	}
	struct stat st;
	fstat(fd, &st);
	if ((st.st_mode & S_IFMT) == S_IFDIR) {
		delete_dir(fd);
		close(fd);
		rmdir(path);
	} else {
		close(fd);
		unlink(path);
	}
}
#endif

#ifdef _WIN32
static uint64_t get_file_time(const char *path) {
	HANDLE h = CreateFileA(path, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	uint64_t ret = 0;
	if (h != INVALID_HANDLE_VALUE) {
		FILETIME mtime;
		if (GetFileTime(h, NULL, NULL, &mtime)) {
			ret = (uint64_t)mtime.dwLowDateTime | (((uint64_t)mtime.dwHighDateTime) << 32);
		}
		CloseHandle(h);
	}
	return ret;
}
static int is_done_newer() {
	uint64_t tmanifest = get_file_time("download.manifest");
	uint64_t tdone = get_file_time("download.done");
	return tmanifest && tdone && tdone > tmanifest;
}
#else
static int is_done_newer() {
	struct stat in, out;
	return !stat("download.manifest", &in)
		&& !stat("download.done", &out)
		&& out.st_mtimespec.tv_sec > in.st_mtimespec.tv_sec;
}
#endif

static inline int ends_with(const char *s, size_t sz, const char *test) {
	return sz >= strlen(test)
		&& !strcmp(s + sz - strlen(test), test);
}

int main(int argc, char *argv[]) {
	if (argc > 1 && chdir(argv[1])) {
		fprintf(stderr, "failed to change directory to %s\n", argv[1]);
		return 1;
	}
	if (is_done_newer()) {
		fprintf(stderr, "download.exe: no work to do\n");
		return 0;
	}
	FILE *inf = fopen("download.manifest", "r");
	if (inf == NULL) {
		fprintf(stderr, "could not open download.manifest\n");
		return 2;
	}

	char line[256];
	while (fgets(line, sizeof(line), inf)) {
		char *end = line + strlen(line);
		if (end == line || end[-1] != '\n') {
			fprintf(stderr, "overlong line\n");
			return 3;
		}

		char *p = trim(line);
		if (*p == '#' || *p == '\0') {
			// comment or empty line
			continue;
		}

		char *path = strtok(p, ",");
		char *url = strtok(NULL, ",");
		char *platform = strtok(NULL, ",");

		if (!is_host_platform(platform)) {
			continue;
		}

		if (!path || !url) {
			fprintf(stderr, "invalid line\n");
			return 3;
		}

		path = trim(path);
		url = trim(url);

		fprintf(stderr, "checking %s\n", path);

		if (has_download_done(path, url)) {
			continue;
		}

		fprintf(stderr, "downloading %s from %s\n", path, url);
		delete_path(path);

		stream *s;
		FILE *f = NULL;

		if (!strncmp(url, "http://", strlen("http://"))
		|| !strncmp(url, "https://", strlen("https://"))) {
			s = open_http_downloader(url);

		} else if (!strncmp(url, "file:", strlen("file:"))) {
			const char *source = url + strlen("file:");
			f = fopen(source, "rb");
			if (!f) {
				fprintf(stderr, "failed to open file %s\n", source);
				return 6;
			}
			s = open_file_source(f);

		} else {
			fprintf(stderr, "unknown scheme for %s\n", url);
			return 4;
		}

		if (!s) {
			return 5;
		}

		size_t urlsz = strlen(url);
		if (ends_with(url, urlsz, ".xz") || ends_with(url, urlsz, ".txz")) {
			if ((s = open_xz_decoder(s)) == NULL) {
				return 5;
			}
		}

		int err;
		if (ends_with(url, urlsz, ".tar.xz") || ends_with(url, urlsz, ".txz")) {
			err = extract_tar(path, s);
		} else if (ends_with(url, urlsz, ".zip")) {
			err = extract_zip(path, s);
		} else {
			err = extract_file(path, s);
		}

		if (err) {
			return 6;
		}
		if (update_done(path, url)) {
			return 7;
		}
	}

	// touch the download file so next time we take the shortcut
	FILE *donef = fopen("download.done", "a");
	if (donef) {
		fclose(donef);
	}

	return 0;
}

