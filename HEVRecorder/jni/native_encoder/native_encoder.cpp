#include <jni.h>
#include <stdio.h>
#include "jni_utils.h"
#include "lenthevcenc.h"

static LENT_HANDLE gHandle;
LENT_param_t gParam;
FILE *gFileOut;

int native_encoder_open()
{
	LENT_param_t *param = &gParam;
	LENT_param_default( param, LENT_PRESET_ULTRAFAST );
	gHandle = LENT_encoder_open( param );
	if (!gHandle ) {
		return -1;
	}

	gFileOut = fopen("/sdcard/test.hm", "rb");
	return 0;
}

int native_encoder_encode()
{
	LENT_picture_t pic, pic_out;
	int i_nal_size, i_nal;
	LENT_nal_t *nal = NULL;
	i_nal_size = LENT_encoder_encode( gHandle, &nal, &i_nal, &pic, &pic_out );
	if( i_nal_size < 0 ) {
		return -1;
	} else if( i_nal_size ) {
		fwrite( nal->p_payload, sizeof(uint8_t), i_nal_size, gFileOut);
	}
	return 0;
}

int native_encoder_close()
{
	LENT_picture_t pic, pic_out;
	int i_nal_size, i_nal;
	LENT_nal_t *nal = NULL;
	while( LENT_encoder_encoding( gHandle ) )
	{
		i_nal_size = LENT_encoder_encode( gHandle, &nal, &i_nal, NULL, &pic_out );
		if( i_nal_size < 0 ) {
			return -1;
		} else if( i_nal_size ) {
			fwrite( nal->p_payload, sizeof(uint8_t), i_nal_size, gFileOut );
		}
	}

	fflush(gFileOut);
	fclose( gFileOut );
	LENT_encoder_close( gHandle );

	return 0;
}

static JNINativeMethod gMethods[] = {
    {"native_encoder_open",         "()I",                              (void *)native_encoder_open},
    {"native_encoder_encode",         "()I",                              (void *)native_encoder_encode},
    {"native_encoder_close",         "()I",                              (void *)native_encoder_close},
};

int register_native_methods(JNIEnv *env) {
	return jniRegisterNativeMethods(env, "pku/shengbin/hevrecorder/RecordingActivity", gMethods, sizeof(gMethods) / sizeof(gMethods[0]));
}
