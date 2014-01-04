LOCAL_PATH := $(call my-dir)

ARCH_ABI := $(TARGET_ARCH_ABI)

#
# Prebuilt Shared library
#
include $(CLEAR_VARS)
LOCAL_MODULE	:= lenthevcenc
LOCAL_SRC_FILES	:= ./lib/$(ARCH_ABI)/liblenthevcenc.so
include $(PREBUILT_SHARED_LIBRARY)
