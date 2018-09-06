#include "stream/stream.h"
#include <stdio.h>

int extract_file(stream *s, const char *dir, const char *path, int mode, uint64_t progress_size);
int extract_container(container *c, const char *dir);
void make_directory(char *filepath);
