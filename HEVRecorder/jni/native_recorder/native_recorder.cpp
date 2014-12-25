// native_recorder.cpp: provides JNI functions for the C++
// video recorder to be used in Java code.
//
// Copyright (c) 2013 Strongene Ltd. All Right Reserved.
// http://www.strongene.com
//
// Contributors:
// Shengbin Meng <shengbinmeng@gmail.com>
//
// You are free to re-use this as the basis for your own application
// in source and binary forms, with or without modification, provided
// that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright
// notice and this list of conditions.
//  * Redistributions in binary form must reproduce the above
// copyright notice and this list of conditions in the documentation
// and/or other materials provided with the distribution.

#include "jni_utils.h"
#include "video_recorder.h"
#include "hls_video_recorder.h"

#define LOG_TAG "native_recorder"

static VideoRecorder *recorder = NULL;
static int frameCount = 0;

int native_recorder_open(JNIEnv *env, jobject thiz, jint width, jint height, jstring path)
{
	recorder = new VideoRecorder();

	// support HLS
	const char* p = env->GetStringUTFChars(path, 0);
	if (p[strlen(p)-1] == '/') {
		delete recorder;
		recorder = new HlsVideoRecorder();
	}

	int ret = recorder->setAudioOptions(AudioSampleFormatS16, 2, 44100, 64000);
	if (ret < 0) {
		LOGE("set audio options failed \n");
		return ret;
	}
	ret = recorder->setVideoOptions(VideoFrameFormatYUV420P, width, height, 400000);
	if (ret < 0) {
		LOGE("set video options failed \n");
		return ret;
	}

	const char* filepath = env->GetStringUTFChars(path, 0);
	ret = recorder->open(filepath, true);
	if (ret < 0) {
		LOGE("open recorder failed (%s) \n", filepath);
		return ret;
	}
	frameCount = 0;
	return 0;
}

int native_recorder_encode_video(JNIEnv *env, jobject thiz, jbyteArray array)
{
	// skip frames if encoding is slow
	if ((frameCount % 1) != 0) {
		frameCount++;
		return 0;
	}
	jbyte* data = env->GetByteArrayElements(array, 0);
	jsize length = env->GetArrayLength(array);
	int ret = recorder->supplyVideoFrame(data, length, frameCount);
	frameCount++;
	env->ReleaseByteArrayElements(array, data, 0);
	return ret;
}

int native_recorder_encode_audio(JNIEnv *env, jobject thiz, jbyteArray array)
{
	jbyte* data = env->GetByteArrayElements(array, 0);
	jsize length = env->GetArrayLength(array);
	int ret = recorder->supplyAudioSamples(data, length);
	env->ReleaseByteArrayElements(array, data, 0);
	return ret;
}

int native_recorder_close()
{
	int ret = recorder->close();
	if (ret < 0) {
		LOGE("close recorder failed \n");
	}
	delete recorder;
	return ret;
}

static JNINativeMethod gMethods[] = {
    {"native_recorder_open", "(IILjava/lang/String;)I", (void *)native_recorder_open},
    {"native_recorder_encode_video", "([B)I", (void *)native_recorder_encode_video},
    {"native_recorder_encode_audio", "([B)I", (void *)native_recorder_encode_audio},
    {"native_recorder_close", "()I", (void *)native_recorder_close},
};

int register_native_methods(JNIEnv *env)
{
	return jniRegisterNativeMethods(env, "pku/shengbin/hevrecorder/RecordingActivity", gMethods, sizeof(gMethods) / sizeof(gMethods[0]));
}
