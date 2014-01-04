LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../lenthevcenc/include/

LOCAL_SRC_FILES := native_encoder.cpp jni_utils.cpp

LOCAL_LDLIBS := -llog

LOCAL_SHARED_LIBRARY += lenthevcenc

LOCAL_MODULE := native_encoder

include $(BUILD_SHARED_LIBRARY)
