#include "extract.h"

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

int extract_file(stream *s, char *path, uint64_t progress_size) {
	make_directory(path);

	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "failed to open %s\n", path);
		return -1;
	}

	uint64_t recvd = 0;
	int last_percent = 0;
	if (progress_size > 1024 * 1024) {
		printf("downloading %"PRIu64" MB  0%%", progress_size / 1024 / 1024);
	} else if (progress_size > 1024) {
		printf("downloading %"PRIu64" kB  0%%", progress_size / 1024);
	} else if (progress_size) {
		printf("downloading %"PRIu64" B  0%%", progress_size);
	}
#ifdef WIN32
	printf("\n");
#endif
	fflush(stdout);

	for (;;) {
		size_t len, atend;
		uint8_t *p = s->buffered(s, &len, &atend);
		if (len) {
			if (fwrite(p, 1, len, f) != len) {
				fprintf(stderr, "failed to write output file\n");
				fclose(f);
				return -1;
			}
			s->consume(s, len);
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
		} else if (atend) {
#ifndef WIN32
			if (progress_size) {
				printf("\n");
				fflush(stdout);
			}
#endif
			fclose(f);
			return 0;
		} else if (s->get_more(s)) {
			return -1;
		}
	}
}

