LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := RenderingPlugin
LOCAL_SRC_FILES := RenderingPlugin.cpp
LOCAL_LDLIBS := -llog -lGLESv2
LOCAL_CFLAGS := -DUNITY_ANDROID

include $(BUILD_SHARED_LIBRARY)
