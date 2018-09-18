#include "extract.h"

#ifdef WIN32
#include <direct.h>
#endif

void make_directory(char *file) {
	char *next = file;
	for (;;) {
		char *slash = strchr(next, '/');
		if (!slash) {
			return;
		}
		*slash = 0;
#ifdef WIN32
		_mkdir(file);
#else
		mkdir(file, 0755);
#endif
		*slash = '/';
		next = slash + 1;
	}
}

FILE *open_path(const char *dir, const char *file, int mode) {
	char *path = malloc(strlen(dir) + 1 + strlen(file) + 1);
	if (!path) {
		return NULL;
	}
	strcpy(path, dir);
	strcat(path, "/");
	strcat(path, file);
	make_directory(path);

	FILE *f = fopen(path, "wb");
#ifndef WIN32
	if (f) {
		fchmod(fileno(f), mode);
	}
#endif
	free(path);
	return f;
}

int extract_path(stream *s, const char *dir, const char *file, int mode) {
	FILE *f = open_path(dir, file, mode);
	if (!f) {
		return -1;
	}
	printf("extracting %s/%s\n", dir, file);
	int err = extract_file(s, f, 0);
	fclose(f);
	return err;
}

int extract_file(stream *s, FILE *f, uint64_t progress_size) {
	uint64_t recvd = 0;
	int last_percent = 0;
	if (progress_size > 1024 * 1024) {
		printf("downloading %"PRIu64" MB  0%%", progress_size / 1024 / 1024);
	} else if (progress_size > 1024) {
		printf("downloading %"PRIu64" kB  0%%", progress_size / 1024);
	} else if (progress_size) {
		printf("downloading %"PRIu64" B  0%%", progress_size);
	}
	if (progress_size) {
#ifdef WIN32
		printf("\n");
#endif
		fflush(stdout);
	}

	size_t len = 0;
	for (;;) {
		const uint8_t *p = s->read(s, len, 1, &len);
		if (!len) {
#ifndef WIN32
			if (progress_size) {
				printf("\n");
				fflush(stdout);
			}
#endif
			return 0;
		}

		if (fwrite(p, 1, len, f) != len) {
			fprintf(stderr, "failed to write output file\n");
			return -1;
		}
		recvd += len;
		if (progress_size) {
			int percent = (int)(recvd * 100 / progress_size);
			if (percent != last_percent) {
#ifdef WIN32
				printf("%2d%%\n", percent);
#else
				printf("\b\b\b%2d%%", percent);
#endif
				fflush(stdout);
				last_percent = percent;
			}
		}
	}
}

int extract_container(container *c, const char *dir) {
	for (;;) {
		{
			int err = c->next_file(c);
			if (err < 0) {
				return err;
			} else if (err > 0) {
				return 0;
			}
		}
		if (c->link_target) {
#ifndef WIN32
			printf("creating link from %s/%s to %s\n", dir, c->file_path, c->link_target);
			char *path = malloc(strlen(dir) + 1 + strlen(c->file_path) + 1);
			if (!path) {
				return -1;
			}
			strcpy(path, dir);
			strcat(path, "/");
			strcat(path, c->file_path);
			if (symlink(c->link_target, path)) {
				free(path);
				return -1;
			}
			free(path);
#endif
		} else {
			stream *s = c->open_file(c);
			if (!s) {
				return -1;
			}
			int err = extract_path(s, dir, c->file_path, c->file_mode);
			s->close(s);
			if (err) {
				return err;
			}
		}
	}
}
