#include <dlfcn.h>
#include <stdio.h>

typedef void (*fn_type)(const char *arg);

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "usage: so-loader <dll.so>\n");
		return 2;
	}
	void *dll = dlopen(argv[1], 0);
	if (!dll) {
		fprintf(stderr, "failed to open dll %s\n", dlerror());
		perror("failed to open dll");
		return 3;
	}
	void *ptr = dlsym(dll, "test_dll");
	if (!ptr) {
		fprintf(stderr, "failed to find 'test_dll' function\n");
		return 4;
	}
	fn_type fn = (fn_type)ptr;
	fn("from loader");
	dlclose(dll);
	return 0;
}
