LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	pthreadstest.c

LOCAL_MODULE := beskidtko

include $(BUILD_EXECUTABLE)
