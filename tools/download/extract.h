#include "stream/stream.h"
#include <stdio.h>

FILE *open_path(const char *dir, const char *file, int mode);
int extract_path(stream *s, const char *dir, const char *file, int mode);
int extract_file(stream *s, FILE *f, uint64_t progress_size);
int extract_container(container *c, const char *dir);
void make_directory(char *filepath);
