#
# Copyright (C) 2024 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

LOCAL_PATH := $(call my-dir)

ifeq ($(TARGET_DEVICE),kirby)

include $(CLEAR_VARS)
LOCAL_MODULE := kirby-vendor
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_OWNER := lenovo

LOCAL_SRC_FILES := proprietary-files.txt

include $(BUILD_SYSTEM)/base_rules.mk

$(LOCAL_BUILT_MODULE): $(LOCAL_PATH)/proprietary-files.txt
	@echo "Copying proprietary files for kirby..."
	$(hide) mkdir -p $(TARGET_COPY_OUT_VENDOR)
	$(hide) $(LOCAL_PATH)/../device/lenovo/kirby/extract-files.py

endif

