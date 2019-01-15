#include <stdio.h>

extern void __attribute__((visibility("default"))) test_dll(const char *arg);

void test_dll(const char *arg) {
	fprintf(stderr, "hello from dll %s\n", arg);
}
