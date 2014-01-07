#include <jni.h>
#include <stdio.h>
#include <time.h>
#include "jni_utils.h"
extern "C" {
#include "lenthevcenc.h"
}

#define LOG_TAG "native_encoder"

static LENT_HANDLE gHandle;
static LENT_param_t gParam;
static FILE *gFileOut;

int native_encoder_open()
{
	LOGI("open encoder \n");
	int i_width = 352, i_height = 288;

	LENT_param_t *param = &gParam;
	LENT_param_default( param, LENT_PRESET_ULTRAFAST );
	param->spatial[0].i_width = i_width;
	param->spatial[0].i_height = i_height;

	param->rc.i_rc_method = LENT_RC_ABR;
	param->spatial[0].i_bitrate[0] = 800; //kbps

	param->i_threads = 1;
	param->i_threads_wpp = 1;
	param->i_idr_max = param->i_idr_min = 25;
	param->i_fps_num = 15000;
	param->i_fps_den = 1000;

	param->i_compatibility = 120;

	gHandle = LENT_encoder_open(param);

	if (!gHandle) {
		LOGE("open encoder failed! \n");
		return -1;
	}

	char filename[1024], timenow[100];
	time_t now = time(0);
	strftime(timenow, 100, "%Y-%m-%d-%H-%M-%S", localtime (&now));
	sprintf(filename, "/sdcard/record-%s.hevc", timenow);
	gFileOut = fopen(filename, "wb");
	if (gFileOut == NULL) {
		LOGE("open output file failed! \n");
		return -1;
	}

	return 0;
}

int native_encoder_encode(JNIEnv *env, jobject thiz, jbyteArray array)
{
	LENT_param_t *param = &gParam;
	int width = param->spatial[param->i_spatial_layer - 1].i_width;
	int height = param->spatial[param->i_spatial_layer - 1].i_height;

	jbyte* data = env->GetByteArrayElements(array, NULL);
	jsize length = env->GetArrayLength(array);

	int stride_y = (width % 16 == 0 ? width/16 : width/16 + 1)*16;
	int stride_uv = (width/2 % 16 == 0 ? width/2 / 16 : width/2 / 16 + 1)*16;
	LENT_picture_t pic, pic_out;
	pic.img.i_width[0] = width;
	pic.img.i_width[1] = pic.img.i_width[2] = width/2;
	pic.img.i_height[0] = height;
	pic.img.i_height[1] = pic.img.i_height[2] = height / 2;
	pic.img.i_stride[0] = stride_y;
	pic.img.i_stride[1] = pic.img.i_stride[2] = stride_uv;
	pic.img.plane[0] = (uint8_t*)data;
	pic.img.plane[1] = (uint8_t*)(data + height * stride_y);
	pic.img.plane[2] = (uint8_t*)(data + height * stride_y + height/2 * stride_uv);

	LOGD("encode a pic: %d, %d, %d, %d, %d%d%d%d%d \n", pic.img.i_width[0], pic.img.i_height[0], pic.img.i_stride[0], pic.img.i_stride[1],
			data[0], data[1], data[2], data[3], data[4]);
	int i_nal_size, i_nal;
	LENT_nal_t *nal = NULL;
	i_nal_size = LENT_encoder_encode(gHandle, &nal, &i_nal, &pic, &pic_out);
	if (i_nal_size < 0) {
		return -1;
	} else if (i_nal_size > 0) {
		LOGD("before writing \n");
		fwrite( nal->p_payload, sizeof(uint8_t), i_nal_size, gFileOut);
		LOGD("%d bytes written \n", i_nal_size);
	}

	env->ReleaseByteArrayElements(array, data, 0);
	return 0;
}

int native_encoder_close()
{
	LOGI("close encoder \n");
	int i_nal_size, i_nal;
	LENT_nal_t *nal = NULL;
	LENT_picture_t pic, pic_out;

	while (LENT_encoder_encoding(gHandle)) {
		i_nal_size = LENT_encoder_encode(gHandle, &nal, &i_nal, NULL, &pic_out);
		if (i_nal_size < 0) {
			return -1;
		} else if (i_nal_size > 0) {
			fwrite( nal->p_payload, sizeof(uint8_t), i_nal_size, gFileOut );
			LOGD("%d bytes written \n", i_nal_size);
		}
	}

	fflush(gFileOut);
	fclose(gFileOut);
	LENT_encoder_close(gHandle);

	return 0;
}

static JNINativeMethod gMethods[] = {
    {"native_encoder_open", "()I", (void *)native_encoder_open},
    {"native_encoder_encode", "([B)I", (void *)native_encoder_encode},
    {"native_encoder_close", "()I", (void *)native_encoder_close},
};

int register_native_methods(JNIEnv *env)
{
	return jniRegisterNativeMethods(env, "pku/shengbin/hevrecorder/RecordingActivity", gMethods, sizeof(gMethods) / sizeof(gMethods[0]));
}
