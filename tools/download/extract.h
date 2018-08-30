#include "stream/stream.h"
#include <stdio.h>

int extract_file(stream *s, char *path, uint64_t progress_size);
int extract_zip(FILE *f, const char *dir);
int extract_tar(stream *s, const char *dir);
void make_directory(char *filepath);
char *clean_path(const char *dir, const char *name1, const char *name2);
