#include "stream.h"


static int valid_path(const char *name) {
	if (strchr(name, ':') || *name == '/' || strchr(name, '\\')) {
		return 0;
	}
	// verify the path isn't trying to escape
	const char *next = name;
	for (;;) {
		if (!strcmp(next, "..") || !strncmp(next, "../", 3)) {
			return 0;
		}
		const char *slash = strchr(next, '/');
		if (!slash) {
			break;
		}
		next = slash + 1;
	}
	return 1;
}

char *clean_path(const char *dir, const char *name1, const char *name2) {
	if (!*dir || !valid_path(name1) || !valid_path(name2) || (!*name1 && !*name2)) {
		return NULL;
	}
	char *ret = (char*) malloc(strlen(dir) + 1 + strlen(name1) + 1 + strlen(name2) + 1);
	strcpy(ret, dir);
	if (*name1) {
		strcat(ret, "/");
		strcat(ret, name1);
	}
	if (*name2) {
		strcat(ret, "/");
		strcat(ret, name2);
	}
	return ret;
}
