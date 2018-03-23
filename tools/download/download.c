#include "stream.h"
#include "tar.h"
#include "zip.h"


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

// We keep output map files. They are a series of key value pairs stored as comma seperated values.
#define NOVAL -1
#define MATCH 0
#define MISMATCH 1
static int test_map(const char *file, const char *key, const char *value) {
	FILE *f = fopen(file, "r");
	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			char *fkey = trim(strtok(line, ","));
			char *fval = trim(strtok(NULL, ","));
			if (fkey && fval && !strcmp(fkey, key)) {
				fclose(f);
				return strcmp(fval, value) ? MISMATCH : MATCH;
			}
		}
		fclose(f);
	}
	return NOVAL;
}

static int update_map(const char *file, const char *key, const char *val) {
	FILE *tmpf = fopen("download.map", "w");
	if (tmpf == NULL) {
		fprintf(stderr, "failed to open temp output file\n");
		return -1;
	}
	FILE *f = fopen(file, "r");
	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			char *fkey = trim(strtok(line, ","));
			char *fval = trim(strtok(NULL, ","));
			if (fkey && fval && strcmp(fkey, key)) {
				fprintf(tmpf, "%s,%s\n", fkey, fval);
			}
		}
		fclose(f);
		unlink(file);
	}
	fprintf(tmpf, "%s,%s\n", key, val);
	fclose(tmpf);
	if (rename("download.map", file)) {
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

		if (test_map("download.done", path, url) == MATCH) {
			continue;
		}

		fprintf(stderr, "downloading %s from %s\n", path, url);
		delete_path(path);

		FILE *f = NULL;

		if (!strncmp(url, "http://", strlen("http://"))
		|| !strncmp(url, "https://", strlen("https://"))) {
			uint64_t sz;
			char hash[HASH_BUFSZ];
			stream *s = open_http_downloader(url, &sz);
			if (s == NULL) {
				return 5;
			}
			s = open_sha256_hash(s, hash);
			if (extract_file(s, "download.tmp", sz)) {
				return -1;
			}

			switch (test_map("download.hash", url, hash)) {
			case MATCH:
				break;
			case MISMATCH:
				fprintf(stderr, "downloaded file does not match hash\n");
				return 10;
			case NOVAL:
				fprintf(stderr, "updating hash for %s\n", url);
				if (update_map("download.hash", url, hash)) {
					return 11;
				}
				break;
			}

			f = fopen("download.tmp", "rb");

		} else if (!strncmp(url, "file:", strlen("file:"))) {
			const char *source = url + strlen("file:");
			f = fopen(source, "rb");

		} else {
			fprintf(stderr, "unknown scheme for %s\n", url);
			return 4;
		}

		if (!f) {
			fprintf(stderr, "failed to open %s\n", url);
			return 5;
		}

		int err;
		size_t urlsz = strlen(url);
		if (ends_with(url, urlsz, ".txz") || ends_with(url, urlsz, ".tar.xz")) {
			stream *s = open_file(f);
			s = open_xz_decoder(s);
			err = extract_tar(s, path);

		} else if (ends_with(url, urlsz, ".tar")) {
			stream *s = open_file(f);
			err = extract_tar(s, path);

		} else if (ends_with(url, urlsz, ".zip")) {
			err = extract_zip(f, path);

		} else if (ends_with(url, urlsz, ".xz")) {
			stream *s = open_file(f);
			s = open_xz_decoder(s);
			err = extract_file(s, path, 0);

		} else {
			stream *s = open_file(f);
			err = extract_file(s, path, 0);
		}

		fclose(f);
		unlink("download.tmp");

		if (err || update_map("download.done", path, url)) {
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

