#include <jni.h>
#include <iostream>

JNIEXPORT void JNICALL Java_HelloJNI_sayHello(JNIEnv *env, jobject thisObj);

void Java_HelloJNI_sayHello(JNIEnv *env, jobject thisObj) {
	std::cerr << "hello from jni" << std::endl;
}

