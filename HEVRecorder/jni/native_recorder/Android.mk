LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := native_recorder
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../ffmpeg-2.0/include/
LOCAL_SRC_FILES := VideoRecorder.cpp jni_utils.cpp
LOCAL_SHARED_LIBRARIES += ffmpeg
LOCAL_LDLIBS := -llog 

include $(BUILD_SHARED_LIBRARY)
