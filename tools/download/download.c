#include "extract.h"
#include "bearssl_wrapper.h"

#ifdef WIN32
#pragma comment(lib, "shell32.lib")
#include <windows.h>
#include <direct.h>
#define chdir _chdir
#else
#include <sys/types.h>
#include <dirent.h>
#endif



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
#if defined WIN32
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

#ifdef WIN32
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
	int fd = open(path, O_RDONLY);
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

static inline int ends_with(const char *s, size_t sz, const char *test) {
	return sz >= strlen(test)
		&& !strcmp(s + sz - strlen(test), test);
}

#ifdef WIN32
HANDLE manifest_file;

static char *open_manifest() {
	HANDLE h = CreateFileA("download.manifest", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	OVERLAPPED ol = { 0 };
	if (h == INVALID_HANDLE_VALUE
	|| !LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 0, MAXDWORD, &ol)) {
		fprintf(stderr, "failed to open manifest\n");
		exit(3);
	}
	DWORD sz = GetFileSize(h, NULL);
	char *ret = malloc(sz+1);
	if (!ret || !ReadFile(h, ret, sz, &sz, NULL)) {
		fprintf(stderr, "failed to read manifest\n");
		exit(5);
	}
	ret[sz] = 0;
	manifest_file = h;
	return ret;
}

static void close_manifest() {
	OVERLAPPED ol = { 0 };
	UnlockFileEx(manifest_file, 0, 0, MAXDWORD, &ol);
	CloseHandle(manifest_file);
}

static void run_ninja(int argc, const char *argv[]) {
	char buf[4096];
	buf[0] = 0;
	for (int i = 0; i < argc; i++) {
		int have_space = strchr(argv[i], ' ') != NULL;
		if (have_space) {
			strncat(buf, "\"", sizeof(buf) - strlen(buf) - 1);
		}
		strncat(buf, argv[i], sizeof(buf) - strlen(buf) - 1);
		if (have_space) {
			strncat(buf, "\" ", sizeof(buf) - strlen(buf) - 1);
		} else {
			strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
		}
	}
	printf("%s\n", buf);
	fflush(stdout);
	fflush(stderr);
	STARTUPINFO si = { 0 };
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = INVALID_HANDLE_VALUE;
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	PROCESS_INFORMATION pi;
	if (!CreateProcessA(NULL, buf, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		fprintf(stderr, "failed to launch ninja\n");
		exit(6);
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}
#else
static int manifest_file;

static char *open_manifest() {
	int fd = open("download.manifest", O_CREAT | O_RDONLY, 0755);
	if (fd < 0 || flock(fd, LOCK_EX)) {
		fprintf(stderr, "failed to open manifest\n");
		exit(3);
	}
	struct stat st = { 0 };
	fstat(fd, &st);
	char *ret = malloc(st.st_size+1);
	if (!ret || read(fd, ret, st.st_size) != st.st_size) {
		fprintf(stderr, "failed to read manifest\n");
		exit(5);
	}
	ret[st.st_size] = 0;
	manifest_file = fd;
	return ret;
}

static void close_manifest() {
	flock(manifest_file, LOCK_UN);
	close(manifest_file);
}

static void run_ninja() {
	// do nothing
}
#endif

int main(int argc, char *argv[]) {
	// run as download.exe folder ninja-exe ninja-arguments
	if (argc > 1 && chdir(argv[1])) {
		fprintf(stderr, "failed to change directory to %s\n", argv[1]);
		return 1;
	}
	char *manifest = open_manifest();
	char *next = manifest;
	while (*next) {
		char *line = next;
		char *nl = strchr(line, '\n');
		if (!nl) {
			nl = line + strlen(line);
			next = nl;
		} else {
			next = nl + 1;
		}

		*nl = 0;
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

		if (test_map("download.done", path, url) == MATCH) {
			continue;
		}

		printf("downloading %s from %s\n", path, url);
		delete_path(path);

		FILE *f = NULL;

		if (!strncmp(url, "http://", strlen("http://"))
		|| !strncmp(url, "https://", strlen("https://"))) {
			uint64_t sz;
			br_sha256_context ctx;
			br_sha256_init(&ctx);
			stream *s = open_hash(open_http_downloader(url, &sz), &ctx.vtable);
			if (s == NULL) {
				return 5;
			}
			f = fopen("download.tmp", "w+b");
			if (!f) {
				return 6;
			}
			if (extract_file(s, f, sz)) {
				return -1;
			}

			uint8_t hash[br_sha256_SIZE];
			br_sha256_out(&ctx, hash);
			char str[128];
			strcpy(str, "sha256:");
			for (int i = 0; i < br_sha256_SIZE; i++) {
				sprintf(str + strlen(str), "%02x", hash[i]);
			}

			switch (test_map("download.hash", url, str)) {
			case MATCH:
				break;
			case MISMATCH:
				fprintf(stderr, "downloaded file does not match hash\n");
				return 10;
			case NOVAL:
				fprintf(stderr, "updating hash for %s\n", url);
				if (update_map("download.hash", url, str)) {
					return 11;
				}
				break;
			}

			fflush(f);
			fclose(f);
			f = fopen("download.tmp", "r+b");
			fseek(f, 0, SEEK_SET);

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
			container *c = open_tar(open_xz_decoder(open_file_stream(f)));
			err = extract_container(c, path);
			c->close(c);

		} else if (ends_with(url, urlsz, ".tar")) {
			container *c = open_tar(open_file_stream(f));
			err = extract_container(c, path);
			c->close(c);

		} else if (ends_with(url, urlsz, ".zip")) {
			container *c = open_zip(f);
			err = extract_container(c, path);
			c->close(c);

		} else if (ends_with(url, urlsz, ".xz")) {
			stream *s = open_file_stream(f);
			s = open_xz_decoder(s);
			err = extract_path(s, ".", path, 0644);
			s->close(s);

		} else {
			stream *s = open_file_stream(f);
			err = extract_path(s, ".", path, 0644);
			s->close(s);
		}

		fclose(f);
		unlink("download.tmp");

		if (err || update_map("download.done", path, url)) {
			return 7;
		}
	}

	if (argc > 2) {
		run_ninja(argc - 2, &argv[2]);
	}
	free(manifest);
	close_manifest();
	return 0;
}

