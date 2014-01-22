#include "jni_utils.h"
#include "video_recorder.h"

#define LOG_TAG "native_recorder"

static VideoRecorder *recorder = NULL;
static uint16_t *sound_buffer;
static int frameCount = 0;


int native_recorder_open(JNIEnv *env, jobject thiz, jint width, jint height)
{
	recorder = new VideoRecorder();
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
	char filename[512], timenow[100];
	time_t now = time(0);
	strftime(timenow, 100, "%Y-%m-%d-%H-%M-%S", localtime (&now));
	sprintf(filename, "/sdcard/xxx-%s.flv", timenow);
	ret = recorder->open(filename, false);
	if (ret < 0) {
		LOGE("open recorder failed \n");
		return ret;
	}
	frameCount = 0;
	return 0;
}

int native_recorder_encode_video(JNIEnv *env, jobject thiz, jbyteArray array)
{
	jbyte* data = env->GetByteArrayElements(array, NULL);
	jsize length = env->GetArrayLength(array);
	int ret = recorder->supplyVideoFrame(data, length, frameCount);
	frameCount++;
	delete data;
	return ret;
}

int native_recorder_encode_audio(JNIEnv *env, jobject thiz, jbyteArray array)
{
	jbyte* data = env->GetByteArrayElements(array, NULL);
	jsize length = env->GetArrayLength(array);
	return recorder->supplyAudioSamples(sound_buffer, length);
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
    {"native_recorder_open", "(II)I", (void *)native_recorder_open},
    {"native_recorder_encode_video", "([B)I", (void *)native_recorder_encode_video},
    {"native_recorder_encode_audio", "([B)I", (void *)native_recorder_encode_audio},
    {"native_recorder_close", "()I", (void *)native_recorder_close},
};

int register_native_methods(JNIEnv *env)
{
	return jniRegisterNativeMethods(env, "pku/shengbin/hevrecorder/RecordingActivity", gMethods, sizeof(gMethods) / sizeof(gMethods[0]));
}
