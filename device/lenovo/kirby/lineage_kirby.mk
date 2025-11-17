#
# Copyright (C) 2024 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from kirby device
$(call inherit-product, device/lenovo/kirby/device.mk)

# Inherit some common LineageOS stuff.
$(call inherit-product, vendor/lineage/config/common_full_tablet.mk)

# Product characteristics
PRODUCT_NAME := lineage_kirby
PRODUCT_DEVICE := kirby
PRODUCT_MANUFACTURER := LENOVO
PRODUCT_BRAND := Lenovo
PRODUCT_MODEL := TB321FU

PRODUCT_GMS_CLIENTID_BASE := android-lenovo

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRODUCT_NAME=TB321FU_PRC \
    PRIVATE_BUILD_DESC="TB321FU_PRC-user 14 UKQ1.240510.002 TB321FU_CN_OPEN_USER_Q00040.0_U_ZUI_16.1.10.027_ST_240929 release-keys"

BUILD_FINGERPRINT := Lenovo/TB321FU_PRC/TB321FU:14/UKQ1.240510.002/ZUI_16.1.10.027_240929_PRC:user/release-keys

