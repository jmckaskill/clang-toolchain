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

char *clean_path(const char *name1, const char *name2) {
	char *ret = (char*) malloc(strlen(name1) + 1 + strlen(name2) + 1);
	*ret = 0;
	if (*name1) {
		strcat(ret, name1);
	}
	if (*name2) {
		strcat(ret, name2);
	}
	if (!*ret || !valid_path(ret)) {
		free(ret);
		return NULL;
	}
	return ret;
}
