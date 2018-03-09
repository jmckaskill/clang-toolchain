#include "stream.h"
#include "tar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#define TAR_BLOCK_SIZE UINT64_C(512)

static int extract_file(const char *path, stream *s) {
	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "failed to open %s\n", path);
		return -1;
	}

	for (;;) {
		int len, atend;
		uint8_t *p = s->buffered(s, &len, &atend);
		if (len) {
			if (fwrite(p, 1, len, f) != len) {
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

static char *clean_tar_path(const char *dir, const char *name) {
	if (strchr(name, '\\') || strchr(name, ':')) {
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

	const char *slash = strchr(name, '/');
	const char *file = slash ? (slash + 1) : (name + strlen(name));
	char *ret = (char*) malloc(strlen(dir) + 1 + strlen(file) + 1);
	strcpy(ret, dir);
	strcat(ret, "/");
	strcat(ret, file);
	return ret;
}

static int extract_tar_file(tar_posix_header *t, stream *s, const char *dir) {
	uint64_t sz = strtoull(t->size, NULL, 8);
	uint64_t extra = ((sz + TAR_BLOCK_SIZE - 1) &~ (TAR_BLOCK_SIZE-1)) - sz;

	char *path = clean_tar_path(dir, t->name);
	if (!path) {
		fprintf(stderr, "invalid tar path %s\n", t->name);
		return -1;
	}

	fprintf(stderr, "extracting %s\n", path);
	FILE *f = fopen(path, "wb");
	free(path);
	if (f == NULL) {
		fprintf(stderr, "failed to open %s\n", path);
		return -1;
	}

	s->consume(s, TAR_BLOCK_SIZE);

	int len, atend;

	while (sz) {
		uint8_t *p = s->buffered(s, &len, &atend);
		if (len > sz) {
			len = sz;
		}
		if (len) {
			size_t w = fwrite(p, 1, len, f);
			if (w != len) {
				fprintf(stderr, "write error\n");
				goto err;
			}
			s->consume(s, len);
			sz -= len;
		} else if (atend) {
			fprintf(stderr, "early close of stream\n");
			goto err;
		} else if (s->get_more(s)) {
			goto err;
		}
	}

	while (extra) {
		s->buffered(s, &len, &atend);
		if (len > extra) {
			len = extra;
		}
		if (len) {
			s->consume(s, len);
			extra -= len;
		} else if (atend) {
			fprintf(stderr, "early close of stream\n");
			goto err;
		} else if (s->get_more(s)) {
			goto err;
		}
	}

	fclose(f);
	return 0;

err:
	fclose(f);
	return -1;
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

static int extract_tar_dir(tar_posix_header *t, const char *dir) {
	char *path = clean_tar_path(dir, t->name);
	if (!path) {
		fprintf(stderr, "invalid dir path %s\n", t->name);
		return -1;
	}
#ifdef _MSC_VER
	int err = _mkdir(path);
#else
	int err = mkdir(path, 0755);
#endif
	free(path);
	return err;
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
			case DIRTYPE:
				err = extract_tar_dir(t, dir);
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

int main() {
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

		if (!strncmp(url, "http://", strlen("http://"))
		|| !strncmp(url, "https://", strlen("https://"))) {
			s = open_http_downloader(url);
		} else if (!strncmp(url, "file:", strlen("file:"))) {
			s = open_file_downloader(url + strlen("file:"));
		} else {
			fprintf(stderr, "unknown scheme for %s\n", url);
			return 4;
		}

		if (!s) {
			return 5;
		}

		const char *ext = strrchr(url, '.');
		if (ext && (!strcmp(ext, ".xz") || !strcmp(ext, ".txz"))) {
			if ((s = open_xz_decoder(s)) == NULL) {
				return 5;
			}
		}

		ext = ext ? strrchr(ext, '.') : NULL;
		int istar = ext && (!strcmp(ext, ".tar") || !strcmp(ext, ".txz") || !strcmp(ext, ".tar.xz"));

		if (istar ? extract_tar(path, s) : extract_file(path, s)) {
			return 6;
		}

		if (update_done(path, url)) {
			return 7;
		}
	}

	return 0;
}

