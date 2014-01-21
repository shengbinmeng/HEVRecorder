#include "jni_utils.h"
#include "video_recorder.h"

#define LOG_TAG "native_recorder"

static VideoRecorder *recorder = NULL;
static uint16_t *sound_buffer;
static int frameCount = 0;


int native_recorder_open()
{
	recorder = new VideoRecorder();
	recorder->setAudioOptions(AudioSampleFormatS16, 2, 44100, 64000);
	recorder->setVideoOptions(VideoFrameFormatYUV420P, 352, 288, 400000);
	char filename[512], timenow[100];
	time_t now = time(0);
	strftime(timenow, 100, "%Y-%m-%d-%H-%M-%S", localtime (&now));
	sprintf(filename, "/sdcard/xxx-%s.flv", timenow);
	recorder->open(filename, false);
	frameCount = 0;
	return 0;
}

int native_recorder_encode_video(JNIEnv *env, jobject thiz, jbyteArray array)
{
	jbyte* data = env->GetByteArrayElements(array, NULL);
	jsize length = env->GetArrayLength(array);

	LOGD("encoder get %d bytes \n", length);
	recorder->supplyVideoFrame(data, length, frameCount);
	frameCount++;
	delete data;
	return 0;
}

int native_recorder_encode_sound(JNIEnv *env, jobject thiz, jbyteArray array)
{
	jbyte* data = env->GetByteArrayElements(array, NULL);
	jsize length = env->GetArrayLength(array);
	recorder->supplyAudioSamples(sound_buffer, length);
	return 0;
}

int native_recorder_close()
{
	recorder->close();
	delete recorder;
	delete sound_buffer;
	return 0;
}

static JNINativeMethod gMethods[] = {
    {"native_recorder_open", "()I", (void *)native_recorder_open},
    {"native_recorder_encode_video", "([B)I", (void *)native_recorder_encode_video},
    {"native_recorder_encode_sound", "([B)I", (void *)native_recorder_encode_sound},
    {"native_recorder_close", "()I", (void *)native_recorder_close},
};

int register_native_methods(JNIEnv *env)
{
	return jniRegisterNativeMethods(env, "pku/shengbin/hevrecorder/RecordingActivity", gMethods, sizeof(gMethods) / sizeof(gMethods[0]));
}
