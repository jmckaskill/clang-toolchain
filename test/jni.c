#include <jni.h>
#include <stdio.h>

JNIEXPORT void JNICALL Java_HelloJNI_sayHello(JNIEnv *env, jobject thisObj);

void Java_HelloJNI_sayHello(JNIEnv *env, jobject thisObj) {
	fprintf(stderr, "hello from jni\n");
}

