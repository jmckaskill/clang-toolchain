#include <string>
#include <iostream>

extern "C" {
	void __attribute__((visibility("default"))) test_dll(const char *arg);

	void test_dll(const char *arg) {
		std::cerr << "hello from dll " << arg << std::endl;
	}

}
