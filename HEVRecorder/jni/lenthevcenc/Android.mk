LOCAL_PATH := $(call my-dir)

#
# Prebuilt shared library
#
include $(CLEAR_VARS)

LOCAL_MODULE	:= lenthevcenc
LOCAL_SRC_FILES	:= lib/liblenthevcenc.so

include $(PREBUILT_SHARED_LIBRARY)
