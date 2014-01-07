LOCAL_PATH := $(call my-dir)

#
# FFmpeg prebuilt static libraries
#
include $(CLEAR_VARS)
LOCAL_MODULE	:= avutil_prebuilt
LOCAL_SRC_FILES	:= lib/libavutil.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE	:= avcodec_prebuilt
LOCAL_SRC_FILES	:= lib/libavcodec.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE	:= avformat_prebuilt
LOCAL_SRC_FILES	:= lib/libavformat.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE	:= avfilter_prebuilt
LOCAL_SRC_FILES	:= lib/libavfilter.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE	:= swscale_prebuilt
LOCAL_SRC_FILES	:= lib/libswscale.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE	:= swresample_prebuilt
LOCAL_SRC_FILES	:= lib/libswresample.a
include $(PREBUILT_STATIC_LIBRARY)


#
# FFmpeg shared library
#
include $(CLEAR_VARS)

LOCAL_MODULE := ffmpeg
LOCAL_WHOLE_STATIC_LIBRARIES := avutil_prebuilt avcodec_prebuilt avformat_prebuilt \
								avfilter_prebuilt swscale_prebuilt swresample_prebuilt

include $(BUILD_SHARED_LIBRARY)