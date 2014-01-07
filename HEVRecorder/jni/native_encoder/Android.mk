LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := native_encoder
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../lenthevcenc/include/
LOCAL_SRC_FILES := native_encoder.cpp jni_utils.cpp
LOCAL_SHARED_LIBRARIES += lenthevcenc
LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)