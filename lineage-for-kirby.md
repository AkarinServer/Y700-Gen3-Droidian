# LEGION Tab Gen 3 (kirby) LineageOS 移植和编译完整指南

## 目录

1. [概述](#概述)
2. [准备工作](#准备工作)
3. [构建环境搭建](#构建环境搭建)
4. [获取 LineageOS 源码](#获取-lineageos-源码)
5. [设备代码移植](#设备代码移植)
6. [内核集成](#内核集成)
7. [提取专有文件 (Proprietary Blobs)](#提取专有文件-proprietary-blobs)
8. [设备树配置](#设备树配置)
9. [编译 LineageOS](#编译-lineageos)
10. [刷入和测试](#刷入和测试)
11. [调试和问题解决](#调试和问题解决)
12. [提交官方支持](#提交官方支持)
13. [参考资料](#参考资料)

---

## 概述

本指南将帮助您为 **LEGION Tab Gen 3**（设备代号：**kirby**，芯片：**骁龙 8 Gen 3**）移植并编译 LineageOS。本项目参考了 **OnePlus Pad Pro (caihong)** 的 LineageOS 编译教程。

### 设备信息

- **设备名称**：LEGION Tab Gen 3
- **设备代号**：kirby
- **芯片平台**：Qualcomm Snapdragon 8 Gen 3 (SM8650)
- **内核版本**：6.1.68
- **参考设备**：OnePlus Pad Pro (caihong)
- **LineageOS 版本**：lineage-23.0 (Android 14)

### 已拥有的资源

- ✅ Kernel 6.1.68 源码（`kernel-6.1.68/`）
- ✅ Qssi 目录（系统级代码）
- ✅ Vendor 目录（供应商相关代码）

---

## 准备工作

### 硬件要求

- **计算机**：
  - x86_64 架构
  - **内存**：至少 32GB（推荐 64GB，LineageOS 23.0 需要更多内存）
  - **存储空间**：至少 300GB 可用空间（推荐 500GB+）
  - **处理器**：多核处理器（编译时间与核心数成反比）

- **设备**：
  - LEGION Tab Gen 3 (kirby)
  - USB 数据线
  - 稳定的电源供应（编译过程可能持续数小时）

### 软件要求

- **操作系统**：Ubuntu 20.04 LTS 或更高版本（推荐 Ubuntu 22.04 LTS）
- **网络**：稳定的高速互联网连接（下载源码需要大量带宽）
- **时间**：首次编译可能需要 4-8 小时，取决于硬件配置

### 知识储备

建议您对以下内容有一定了解：
- Linux 命令行操作
- Git 版本控制
- Android 系统架构基础
- 设备树 (Device Tree) 概念
- 编译系统（AOSP/LineageOS 构建系统）

---

## 构建环境搭建

### 步骤 1：安装平台工具

安装 `adb` 和 `fastboot`：

```bash
# 下载 platform-tools
cd ~
wget https://dl.google.com/android/repository/platform-tools-latest-linux.zip
unzip platform-tools-latest-linux.zip -d ~

# 添加到 PATH
echo '# add Android SDK platform tools to path' >> ~/.profile
echo 'if [ -d "$HOME/platform-tools" ] ; then' >> ~/.profile
echo '    PATH="$HOME/platform-tools:$PATH"' >> ~/.profile
echo 'fi' >> ~/.profile

# 更新环境
source ~/.profile

# 验证安装
adb version
fastboot --version
```

### 步骤 2：安装构建依赖包

```bash
sudo apt update
sudo apt install -y \
    bc \
    bison \
    build-essential \
    ccache \
    curl \
    flex \
    g++-multilib \
    gcc-multilib \
    git \
    git-lfs \
    gnupg \
    gperf \
    imagemagick \
    lib32readline-dev \
    lib32z1-dev \
    libdw-dev \
    libelf-dev \
    liblz4-tool \
    libncurses5-dev \
    libsdl1.2-dev \
    libssl-dev \
    libxml2 \
    libxml2-utils \
    lzop \
    pngcrush \
    rsync \
    schedtool \
    squashfs-tools \
    xsltproc \
    zip \
    zlib1g-dev
```

**Ubuntu 23.10+ 特殊处理**：

```bash
# 安装旧版 libncurses5（Ubuntu 23.10+ 需要）
wget https://archive.ubuntu.com/ubuntu/pool/universe/n/ncurses/libtinfo5_6.3-2_amd64.deb
sudo dpkg -i libtinfo5_6.3-2_amd64.deb
rm -f libtinfo5_6.3-2_amd64.deb

wget https://archive.ubuntu.com/ubuntu/pool/universe/n/ncurses/libncurses5_6.3-2_amd64.deb
sudo dpkg -i libncurses5_6.3-2_amd64.deb
rm -f libncurses5_6.3-2_amd64.deb
```

### 步骤 3：安装 Java 开发工具包

LineageOS 23.0 需要 **OpenJDK 17**：

```bash
sudo apt install -y openjdk-17-jdk

# 验证安装
java -version
# 应显示 openjdk version "17.x.x"
```

### 步骤 4：安装 Python

LineageOS 23.0 需要 **Python 3**：

```bash
sudo apt install -y python3 python3-pip python-is-python3

# 验证安装
python --version
# 应显示 Python 3.x.x
```

### 步骤 5：创建目录结构

```bash
mkdir -p ~/bin
mkdir -p ~/android/lineage
```

### 步骤 6：安装 Repo 工具

```bash
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo

# 验证 ~/bin 在 PATH 中
echo '# set PATH so it includes user'\''s private bin if it exists' >> ~/.profile
echo 'if [ -d "$HOME/bin" ] ; then' >> ~/.profile
echo '    PATH="$HOME/bin:$PATH"' >> ~/.profile
echo 'fi' >> ~/.profile

source ~/.profile

# 验证安装
repo --version
```

### 步骤 7：配置 Git

```bash
git config --global user.email "[email protected]"
git config --global user.name "Your Name"

# 配置 Git LFS（大文件存储）
git lfs install

# 配置 Change-Id trailer（用于 Gerrit）
git config --global trailer.changeid.key "Change-Id"
```

### 步骤 8：配置 ccache（加速编译）

```bash
# 启用 ccache
export USE_CCACHE=1
export CCACHE_EXEC=/usr/bin/ccache

# 添加到 .bashrc
echo 'export USE_CCACHE=1' >> ~/.bashrc
echo 'export CCACHE_EXEC=/usr/bin/ccache' >> ~/.bashrc

# 设置缓存大小（50GB，可根据磁盘空间调整）
ccache -M 50G

# 启用压缩（可选，可以节省空间）
ccache -o compression=true

source ~/.bashrc
```

---

## 获取 LineageOS 源码

### 步骤 1：初始化 LineageOS 仓库

```bash
cd ~/android/lineage

# 初始化 repo（使用 lineage-23.0 分支，与 OnePlus Pad Pro 一致）
repo init -u https://github.com/LineageOS/android.git -b lineage-23.0 --git-lfs --no-clone-bundle
```

**注意**：确保使用 `lineage-23.0` 分支，因为您的内核是 6.1.68，与 Android 14 (LineageOS 23.0) 匹配。

### 步骤 2：同步源码

```bash
# 同步源码（这将花费很长时间，取决于网络速度）
repo sync -c --no-clone-bundle --no-tags --optimized-fetch --prune -j$(nproc --all)
```

**说明**：
- `-c`：只同步当前分支
- `-j$(nproc --all)`：使用所有 CPU 核心并行下载
- 如果网络不稳定，可以减少并行数：`-j4`

**预计时间**：1-4 小时（取决于网络速度）

### 步骤 3：验证源码完整性

```bash
cd ~/android/lineage
source build/envsetup.sh

# 测试构建环境
croot
# 应该切换到 ~/android/lineage 目录
```

---

## 设备代码移植

### 步骤 1：确定设备目录结构

您需要创建以下目录结构：

```
~/android/lineage/
├── device/
│   └── lenovo/
│       └── kirby/              # 设备特定配置
├── kernel/
│   └── lenovo/
│       └── kirby/              # 内核源码
└── vendor/
    └── lenovo/
        └── kirby/              # 专有文件（需要从设备提取）
```

### 步骤 2：集成内核源码

```bash
cd ~/android/lineage

# 创建内核目录
mkdir -p kernel/lenovo/kirby

# 复制内核源码（从您现有的 kernel-6.1.68 目录）
cp -r ~/Y700-Gen3-Droidian/kernel-6.1.68/kernel/* kernel/lenovo/kirby/

# 或者创建符号链接（节省空间）
# ln -s ~/Y700-Gen3-Droidian/kernel-6.1.68/kernel kernel/lenovo/kirby
```

### 步骤 3：创建设备树目录

```bash
cd ~/android/lineage
mkdir -p device/lenovo/kirby
cd device/lenovo/kirby
```

### 步骤 4：创建设备配置文件

您需要创建以下关键文件。由于您参考 OnePlus Pad Pro (caihong)，建议先查看其设备树：

```bash
# 查看参考设备的设备树结构
# 访问：https://github.com/LineageOS/android_device_oneplus_caihong
```

#### 4.1 创建 `AndroidProducts.mk`

```bash
cat > AndroidProducts.mk << 'EOF'
#
# Copyright (C) 2024 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

PRODUCT_MAKEFILES := \
    $(LOCAL_DIR)/lineage_kirby.mk

COMMON_LUNCH_CHOICES := \
    lineage_kirby-user \
    lineage_kirby-userdebug \
    lineage_kirby-eng
EOF
```

#### 4.2 创建 `lineage.dependencies`

```bash
cat > lineage.dependencies << 'EOF'
[
  {
    "repository": "android_kernel_lenovo_kirby",
    "target_path": "kernel/lenovo/kirby",
    "branch": "lineage-23.0",
    "remote": "github"
  },
  {
    "repository": "android_vendor_lenovo_kirby",
    "target_path": "vendor/lenovo/kirby",
    "branch": "lineage-23.0",
    "remote": "github"
  }
]
EOF
```

#### 4.3 创建 `BoardConfig.mk`（关键文件）

这是最复杂的文件，定义了硬件配置。参考 OnePlus Pad Pro 的配置：

```bash
cat > BoardConfig.mk << 'EOF'
#
# Copyright (C) 2024 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/lenovo/kirby

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-2a-dotprod
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := cortex-a76
TARGET_CPU_VARIANT_RUNTIME := cortex-a76

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := generic
TARGET_2ND_CPU_VARIANT_RUNTIME := cortex-a76

# Bootloader
TARGET_BOOTLOADER_BOARD_NAME := kirby
TARGET_NO_BOOTLOADER := true

# Kernel
BOARD_BOOT_HEADER_VERSION := 4
BOARD_KERNEL_CMDLINE := \
    androidboot.hardware=qcom \
    androidboot.memcg=1 \
    androidboot.usbcontroller=a600000.dwc3 \
    cgroup.memory=nokmem \
    loop.max_part=7 \
    lpm_levels.sleep_disabled=1 \
    msm_rtb.filter=0x237 \
    service_locator.enable=1 \
    swiotlb=0 \
    pcie_ports=compat \
    iptable_raw.raw_before_defrag=1 \
    ip6table_raw.raw_before_defrag=1

BOARD_KERNEL_PAGESIZE := 4096
BOARD_KERNEL_BASE := 0x00000000
BOARD_RAMDISK_OFFSET := 0x01000000
BOARD_KERNEL_TAGS_OFFSET := 0x00000100
BOARD_DTB_OFFSET := 0x01f00000
BOARD_MKBOOTIMG_ARGS += --header_version $(BOARD_BOOT_HEADER_VERSION)
BOARD_MKBOOTIMG_ARGS += --pagesize $(BOARD_KERNEL_PAGESIZE) --board ""
BOARD_MKBOOTIMG_ARGS += --ramdisk_offset $(BOARD_RAMDISK_OFFSET)
BOARD_MKBOOTIMG_ARGS += --tags_offset $(BOARD_KERNEL_TAGS_OFFSET)
BOARD_MKBOOTIMG_ARGS += --dtb_offset $(BOARD_DTB_OFFSET)

# Kernel - Source
TARGET_KERNEL_SOURCE := kernel/lenovo/kirby
TARGET_KERNEL_CONFIG := kirby_defconfig
TARGET_KERNEL_CLANG_COMPILE := true
TARGET_KERNEL_ADDITIONAL_FLAGS := DTC_EXT=$(shell pwd)/prebuilts/misc/linux-x86/dtc/dtc

# Kernel - Prebuilt (如果使用预编译内核)
# TARGET_PREBUILT_KERNEL := $(DEVICE_PATH)/prebuilt/kernel

# Partitions
BOARD_FLASH_BLOCK_SIZE := 262144
BOARD_BOOTIMAGE_PARTITION_SIZE := 201326592
BOARD_DTBOIMG_PARTITION_SIZE := 25165824
BOARD_VENDOR_BOOTIMAGE_PARTITION_SIZE := 201326592
BOARD_SUPER_PARTITION_SIZE := 9126805504
BOARD_SUPER_PARTITION_GROUPS := qti_dynamic_partitions
BOARD_QTI_DYNAMIC_PARTITIONS_PARTITION_LIST := system system_ext vendor product odm
BOARD_QTI_DYNAMIC_PARTITIONS_SIZE := 9122611200

BOARD_PARTITION_LIST := $(call to-upper,$(BOARD_QTI_DYNAMIC_PARTITIONS_PARTITION_LIST))
$(foreach p, $(BOARD_PARTITION_LIST), $(eval BOARD_$(p)IMAGE_FILE_SYSTEM_TYPE := erofs))
$(foreach p, $(BOARD_PARTITION_LIST), $(eval TARGET_COPY_OUT_$(p) := $(call to-lower,$(p))))

# File system
BOARD_ROOT_EXTRA_FOLDERS := metadata
BOARD_ROOT_EXTRA_SYMLINKS := /vendor/dsp:/dsp /vendor/firmware_mnt:/firmware /mnt/vendor/persist:/persist
TARGET_FS_CONFIG_GEN := $(DEVICE_PATH)/config.fs

# Properties
TARGET_SYSTEM_PROP += $(DEVICE_PATH)/system.prop
TARGET_VENDOR_PROP += $(DEVICE_PATH)/vendor.prop

# Recovery
BOARD_HAS_LARGE_FILESYSTEM := true
BOARD_HAS_NO_SELECT_BUTTON := true
TARGET_RECOVERY_PIXEL_FORMAT := RGBX_8888
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
BOARD_USES_RECOVERY_AS_BOOT := true
BOARD_BUILD_SYSTEM_ROOT_IMAGE := false
BOARD_MOVE_RECOVERY_RESOURCES_TO_VENDOR_BOOT := true

# Security patch level
VENDOR_SECURITY_PATCH := 2024-11-05

# Verified Boot
BOARD_AVB_ENABLE := true
BOARD_AVB_MAKE_VBMETA_IMAGE_ARGS += --flags 3
BOARD_MOVE_GSI_AVB_KEYS_TO_VENDOR_BOOT := true

BOARD_AVB_RECOVERY_KEY_PATH := external/avb/test/data/testkey_rsa4096.pem
BOARD_AVB_RECOVERY_ALGORITHM := SHA256_RSA4096
BOARD_AVB_RECOVERY_ROLLBACK_INDEX := 1
BOARD_AVB_RECOVERY_ROLLBACK_INDEX_LOCATION := 1

BOARD_AVB_VBMETA_SYSTEM := product system system_ext
BOARD_AVB_VBMETA_SYSTEM_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_VBMETA_SYSTEM_ALGORITHM := SHA256_RSA2048
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX_LOCATION := 2

BOARD_AVB_VBMETA_VENDOR := vendor
BOARD_AVB_VBMETA_VENDOR_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_VBMETA_VENDOR_ALGORITHM := SHA256_RSA2048
BOARD_AVB_VBMETA_VENDOR_ROLLBACK_INDEX := $(PLATFORM_SECURITY_PATCH_TIMESTAMP)
BOARD_AVB_VBMETA_VENDOR_ROLLBACK_INDEX_LOCATION := 3

# SELinux
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/vendor

# WiFi
BOARD_WLAN_DEVICE := qcwcn
BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
WIFI_DRIVER_DEFAULT := qca_cld3
WIFI_DRIVER_STATE_CTRL_PARAM := "/dev/wlan"
WIFI_DRIVER_STATE_OFF := "OFF"
WIFI_DRIVER_STATE_ON := "ON"
WIFI_HIDL_FEATURE_DUAL_INTERFACE := true
WIFI_HIDL_UNIFIED_SUPPLICANT_SERVICE_RC_ENTRY := true
WPA_SUPPLICANT_VERSION := VER_0_8_X

# Inherit from the proprietary version
include vendor/lenovo/kirby/BoardConfigVendor.mk
EOF
```

**重要说明**：
- 分区大小需要根据您的实际设备进行调整
- 请从设备的 `proc/partitions` 或官方固件包中获取准确的分区信息
- `kirby_defconfig` 是您的内核配置文件名称

#### 4.4 创建 `device.mk`

```bash
cat > device.mk << 'EOF'
#
# Copyright (C) 2024 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from sm8650-common
$(call inherit-product, device/lenovo/kirby-common/kirby-common.mk)

# Inherit proprietary blobs
$(call inherit-product, vendor/lenovo/kirby/kirby-vendor.mk)

PRODUCT_NAME := lineage_kirby
PRODUCT_DEVICE := kirby
PRODUCT_MANUFACTURER := Lenovo
PRODUCT_BRAND := LEGION
PRODUCT_MODEL := LEGION Tab Gen 3

PRODUCT_BUILD_PROP_OVERRIDES += \
    TARGET_PRODUCT=$(PRODUCT_NAME)

PRODUCT_GMS_CLIENTID_BASE := android-lenovo
EOF
```

#### 4.5 创建 `lineage_kirby.mk`

```bash
cat > lineage_kirby.mk << 'EOF'
#
# Copyright (C) 2024 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_base_telephony.mk)

# Enable project quotas and casefolding for emulated storage without sdcardfs
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)

# Inherit from kirby device
$(call inherit-product, device/lenovo/kirby/device.mk)

# Inherit some common LineageOS stuff.
$(call inherit-product, vendor/lineage/config/common_full_tablet.mk)

PRODUCT_NAME := lineage_kirby
PRODUCT_DEVICE := kirby
PRODUCT_BRAND := LEGION
PRODUCT_MANUFACTURER := Lenovo
PRODUCT_MODEL := LEGION Tab Gen 3

PRODUCT_GMS_CLIENTID_BASE := android-lenovo

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRIVATE_BUILD_DESC="kirby-user 14 UQ1A.240905.001 eng.xxx.20241112.120000 release-keys"

BUILD_FINGERPRINT := LEGION/kirby/kirby:14/UQ1A.240905.001/xxx12120000:user/release-keys
EOF
```

#### 4.6 创建 `vendorsetup.sh`

```bash
cat > vendorsetup.sh << 'EOF'
add_lunch_combo lineage_kirby-userdebug
EOF
```

#### 4.7 创建其他必要文件

```bash
# 创建空的 system.prop（需要根据设备调整）
touch system.prop

# 创建空的 vendor.prop（需要根据设备调整）
touch vendor.prop

# 创建空的 config.fs
touch config.fs

# 创建 sepolicy 目录
mkdir -p sepolicy/vendor
```

### 步骤 5：创建 common 配置（可选但推荐）

如果多个设备共享配置，可以创建 common 目录：

```bash
cd ~/android/lineage/device/lenovo
mkdir -p kirby-common
cd kirby-common

# 创建 kirby-common.mk 等文件
# 参考其他设备的 common 配置
```

---

## 内核集成

### 步骤 1：配置内核构建

确保您的内核配置正确：

```bash
cd ~/android/lineage/kernel/lenovo/kirby

# 检查 defconfig 是否存在
ls arch/arm64/configs/kirby_defconfig

# 如果不存在，从 vendor 配置生成
# make ARCH=arm64 kirby_defconfig
```

### 步骤 2：调整内核配置

可能需要在内核配置中启用/禁用某些选项以匹配 LineageOS 要求：

```bash
cd ~/android/lineage/kernel/lenovo/kirby
make ARCH=arm64 kirby_defconfig
make ARCH=arm64 menuconfig

# 根据需要调整配置，然后保存
```

### 步骤 3：验证内核编译

```bash
cd ~/android/lineage
source build/envsetup.sh
breakfast kirby

# 如果 breakfast 成功，说明配置基本正确
```

---

## 提取专有文件 (Proprietary Blobs)

### 方法 1：从运行中的设备提取（推荐）

**前提**：设备已运行官方固件或基于相同 Android 版本的固件。

#### 1.1 准备设备

```bash
# 连接设备
adb devices

# 确保设备已 root
adb root
adb remount
```

#### 1.2 运行提取脚本

```bash
cd ~/android/lineage/device/lenovo/kirby

# 如果没有 extract-files.sh，创建一个
cat > extract-files.sh << 'EOF'
#!/bin/bash

set -e

export DEVICE=kirby
export VENDOR=lenovo

if [ -z "${ANDROID_BUILD_TOP}" ]; then
    export ANDROID_BUILD_TOP="$(dirname "$(dirname "$(dirname "$(readlink -f "$0")")")")"
fi

HELPER="${ANDROID_BUILD_TOP}/vendor/lineage/build/tools/extract_utils.sh"
if [ ! -f "${HELPER}" ]; then
    echo "Unable to find helper script at ${HELPER}"
    exit 1
fi

source "${HELPER}"

setup_vendor "${DEVICE}" "${VENDOR}" "${ANDROID_BUILD_TOP}" false false

extract "${ANDROID_BUILD_TOP}/device/${VENDOR}/${DEVICE}/proprietary-files.txt" \
    "${SRC}" \
    "${ANDROID_BUILD_TOP}" \
    "${KANG}" \
    --section "${SECTION}"

"${ANDROID_BUILD_TOP}/vendor/lineage/build/tools/setup-makefiles.sh"
EOF

chmod +x extract-files.sh

# 创建 proprietary-files.txt（需要根据设备填充）
touch proprietary-files.txt
```

#### 1.3 创建 proprietary-files.txt

这是关键文件，列出需要提取的所有专有文件。参考格式：

```bash
cat > proprietary-files.txt << 'EOF'
# Audio
vendor/lib/libaudioalsa.so
vendor/lib64/libaudioalsa.so

# Camera
vendor/lib/libcamera2ndk_vendor.so
vendor/lib64/libcamera2ndk_vendor.so

# Display
vendor/lib/libqdMetaData.so
vendor/lib64/libqdMetaData.so

# 更多文件...
EOF
```

**如何获取文件列表**：

1. 参考类似设备的 `proprietary-files.txt`
2. 从设备的 `/system` 和 `/vendor` 分区枚举文件
3. 使用 `adb pull` 提取整个分区进行分析

#### 1.4 执行提取

```bash
./extract-files.sh
```

文件将被提取到 `~/android/lineage/vendor/lenovo/kirby/`。

### 方法 2：从官方固件包提取

如果您有官方固件包（如 OTA 更新包或工厂镜像）：

```bash
# 解压固件包
unzip -q firmware.zip -d firmware_extracted/

# 使用工具提取系统镜像
# 例如使用 simg2img 工具
simg2img system.img system_ext4.img
sudo mount -o ro system_ext4.img /mnt/system

# 复制文件到 vendor 目录
mkdir -p ~/android/lineage/vendor/lenovo/kirby/proprietary
sudo cp -r /mnt/system/vendor/* ~/android/lineage/vendor/lenovo/kirby/proprietary/
```

### 步骤 3：创建 vendor makefile

```bash
cd ~/android/lineage/vendor/lenovo/kirby

# 创建 Android.mk 或 Android.bp
cat > kirby-vendor.mk << 'EOF'
$(call inherit-product, vendor/lenovo/kirby/kirby-vendor-blobs.mk)
EOF

# 创建 BoardConfigVendor.mk
cat > BoardConfigVendor.mk << 'EOF'
# 空的，由 extract-files.sh 自动生成
EOF
```

运行 `extract-files.sh` 会自动生成必要的 makefile。

---

## 设备树配置

### 步骤 1：获取设备树源文件 (DTS)

设备树文件通常在内核源码的 `arch/arm64/boot/dts/qcom/` 目录中：

```bash
cd ~/android/lineage/kernel/lenovo/kirby
find arch/arm64/boot/dts -name "*kirby*.dts*" -o -name "*kirby*.dtsi*"
```

### 步骤 2：验证设备树编译

```bash
cd ~/android/lineage/kernel/lenovo/kirby
make ARCH=arm64 kirby_defconfig
make ARCH=arm64 dtbs

# 检查是否生成了 dtb 文件
find . -name "*.dtb" -path "*/kirby/*"
```

### 步骤 3：确保设备树文件正确

确保 `BoardConfig.mk` 中的设备树路径正确：

```makefile
BOARD_PREBUILT_DTBIMAGE_DIR := $(TARGET_KERNEL_SOURCE)/arch/arm64/boot/dts/qcom
BOARD_PREBUILT_DTBOIMAGE := $(TARGET_KERNEL_SOURCE)/arch/arm64/boot/dts/qcom/kirby.dtbo
```

---

## 编译 LineageOS

### 步骤 1：准备构建环境

```bash
cd ~/android/lineage

# 设置构建环境
source build/envsetup.sh

# 选择设备
breakfast kirby
# 或者直接
lunch lineage_kirby-userdebug
```

如果 `breakfast` 或 `lunch` 失败，检查：
1. 设备树文件是否正确
2. `lineage.dependencies` 配置是否正确
3. 是否有语法错误

### 步骤 2：首次同步依赖（如果需要）

```bash
# 如果使用了 lineage.dependencies，运行：
repo sync -c --no-clone-bundle --no-tags --optimized-fetch --prune kernel/lenovo/kirby vendor/lenovo/kirby
```

### 步骤 3：开始编译

```bash
# 方法 1：完整编译（推荐首次编译）
brunch kirby

# 方法 2：使用 mka（利用所有 CPU 核心）
mka bacon

# 方法 3：单线程编译（调试时使用）
make -j1 bacon
```

**编译时间**：
- 首次编译：4-8 小时（取决于硬件）
- 增量编译：30 分钟 - 2 小时

### 步骤 4：编译输出

编译成功后，输出文件位于：

```bash
cd ~/android/lineage/out/target/product/kirby/

# 主要文件：
# - lineage-23.0-YYYYMMDD-UNOFFICIAL-kirby.zip  # 系统安装包
# - recovery.img                                 # Recovery 镜像
# - boot.img                                     # Boot 镜像
# - dtbo.img                                     # Device Tree Overlay
# - vendor_boot.img                              # Vendor Boot 镜像
```

---

## 刷入和测试

### ⚠️ 警告

**刷机有风险，请确保：**
1. 已备份所有重要数据
2. Bootloader 已解锁
3. 已了解如何进入 Recovery/Fastboot 模式
4. 有官方固件包可用于救砖

### 步骤 1：解锁 Bootloader

```bash
# 1. 启用开发者选项
# 设置 -> 关于平板 -> 连续点击版本号 7 次

# 2. 启用 OEM 解锁和 USB 调试
# 设置 -> 开发者选项 -> 启用 OEM 解锁和 USB 调试

# 3. 重启到 bootloade