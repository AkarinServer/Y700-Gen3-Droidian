# LineageOS for LEGION Tab Gen 3 (TB321FU / kirby)

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Android](https://img.shields.io/badge/Android-14-green.svg)](https://source.android.com/)
[![Kernel](https://img.shields.io/badge/Kernel-6.1.68-orange.svg)](kernel/lenovo/kirby/)

LineageOS 移植项目，适用于联想拯救者平板 Y700 Gen 3 (TB321FU)，代号 kirby。

## 📱 设备信息

- **设备型号**: TB321FU
- **设备代号**: kirby
- **制造商**: Lenovo
- **平台**: Qualcomm Snapdragon 8 Gen 3 (SM8650 / pineapple)
- **Android 版本**: 14 (API 34)
- **内核版本**: 6.1.68
- **屏幕**: 1600x2560, 400 DPI

## 🚀 快速开始

### 一键配置脚本（推荐）

如果你刚安装完 Ubuntu 22.04，可以使用自动化脚本快速配置环境：

```bash
# 克隆项目
git clone https://github.com/AkarinServer/Y700-Gen3-Droidian.git
cd Y700-Gen3-Droidian

# 运行配置脚本
chmod +x setup-ubuntu.sh
./setup-ubuntu.sh
```

脚本会自动安装所有依赖并配置环境。完成后按照下面的步骤继续。

### 系统要求

- **操作系统**: Ubuntu 22.04 LTS (推荐) 或 Ubuntu 20.04 LTS
- **内存**: 至少 16GB RAM (推荐 32GB)
- **存储空间**: 至少 250GB 可用空间 (推荐 500GB)
- **网络**: 稳定的互联网连接（首次同步需要下载大量数据）

### 第一步：安装系统依赖

在全新的 Ubuntu 22.04 系统上，首先更新系统并安装必要的依赖：

```bash
# 更新系统包列表
sudo apt-get update

# 安装基础构建工具和依赖
sudo apt-get install -y \
    bc \
    bison \
    build-essential \
    ccache \
    curl \
    flex \
    g++-multilib \
    gcc-multilib \
    git \
    gnupg \
    gperf \
    imagemagick \
    lib32ncurses5-dev \
    lib32readline-dev \
    lib32z1-dev \
    liblz4-tool \
    libncurses5 \
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

# 安装 Python 3
sudo apt-get install -y python3 python3-pip

# 安装 Git LFS（用于管理大文件）
sudo apt-get install -y git-lfs

# 安装 Java 17（LineageOS 21.0 需要）
sudo apt-get install -y openjdk-17-jdk

# 安装 Android 工具（可选，用于设备调试）
sudo apt-get install -y android-tools-adb android-tools-fastboot

# 验证安装
git --version
python3 --version
java -version
git lfs version
```

### 第二步：配置 Java 环境

LineageOS 21.0 (Android 14) 需要 Java 17：

```bash
# 设置 Java 环境变量（如果第一步已安装，这里只需配置环境变量）
echo '' >> ~/.bashrc
echo '# Java 17 for LineageOS' >> ~/.bashrc
echo 'export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64' >> ~/.bashrc
echo 'export PATH=$JAVA_HOME/bin:$PATH' >> ~/.bashrc
source ~/.bashrc

# 验证 Java 版本
java -version
# 应该显示: openjdk version "17.x.x"

# 如果系统中有多个 Java 版本，可以设置默认版本
sudo update-alternatives --config java
```

### 第三步：安装 Repo 工具

Repo 是 Google 开发的用于管理多个 Git 仓库的工具：

```bash
# 创建 bin 目录（如果不存在）
mkdir -p ~/bin

# 下载 repo 工具
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo

# 添加到 PATH
echo '' >> ~/.bashrc
echo '# Repo tool' >> ~/.bashrc
echo 'export PATH=~/bin:$PATH' >> ~/.bashrc
source ~/.bashrc

# 验证安装
repo --version
# 应该显示 repo 版本信息
```

### 第四步：配置 Git 和 Git LFS

```bash
# 配置 Git 用户信息（请替换为你的信息）
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"

# 初始化 Git LFS（用于管理大文件）
git lfs install

# 配置 Git 以处理大仓库和网络问题
git config --global core.bigFileThreshold 1
git config --global http.postBuffer 524288000
git config --global http.lowSpeedLimit 0
git config --global http.lowSpeedTime 999999

# 验证 Git LFS
git lfs version
```

### 第五步：初始化 LineageOS 源码

```bash
# 创建工作目录（建议使用大容量分区）
mkdir -p ~/lineageos
cd ~/lineageos

# 初始化 LineageOS 仓库（Android 14 / LineageOS 21.0）
repo init -u https://github.com/LineageOS/android.git -b lineage-21.0

# 同步源码（这需要很长时间，可能需要数小时）
# 首次同步建议使用较少的并发数，避免网络问题
repo sync -c -j4

# 如果网络稳定，可以使用更多并发任务加速
# repo sync -c -j$(nproc --all)
```

**重要提示**:
- 首次同步可能需要 **2-6 小时**，取决于网络速度
- 需要下载约 **200-300GB** 的数据
- 如果同步中断，可以重新运行 `repo sync -c -j4` 继续
- 建议在网络稳定的环境下进行，或使用代理

### 第六步：克隆设备树和内核

```bash
# 进入 LineageOS 源码目录
cd ~/lineageos

# 创建必要的目录结构
mkdir -p device/lenovo kernel/lenovo vendor/lenovo

# 方法 1: 从 GitHub 克隆（推荐）
git clone https://github.com/AkarinServer/Y700-Gen3-Droidian.git temp-kirby
cp -r temp-kirby/device/lenovo/kirby device/lenovo/
cp -r temp-kirby/kernel/lenovo/kirby kernel/lenovo/
cp -r temp-kirby/vendor/lenovo/kirby vendor/lenovo/
rm -rf temp-kirby

# 方法 2: 如果设备树在本地，直接复制
# cp -r /path/to/Y700-Gen3-Droidian/device/lenovo/kirby device/lenovo/
# cp -r /path/to/Y700-Gen3-Droidian/kernel/lenovo/kirby kernel/lenovo/
# cp -r /path/to/Y700-Gen3-Droidian/vendor/lenovo/kirby vendor/lenovo/

# 验证文件已复制
ls -la device/lenovo/kirby/
ls -la kernel/lenovo/kirby/
ls -la vendor/lenovo/kirby/
```

### 第七步：配置构建环境

```bash
cd ~/lineageos

# 初始化构建环境（每次新终端都需要运行）
source build/envsetup.sh

# 选择设备（会自动下载设备相关的依赖）
breakfast kirby

# 如果 breakfast 失败，检查：
# 1. 设备树是否正确放置在 device/lenovo/kirby/
# 2. 内核是否正确放置在 kernel/lenovo/kirby/
# 3. vendor 文件是否存在

# 如果 breakfast 失败，可以手动设置：
# export TARGET_DEVICE=kirby
# export TARGET_PRODUCT=lineage_kirby
# export TARGET_BUILD_VARIANT=userdebug
```

### 第八步：开始构建

```bash
cd ~/lineageos

# 确保已初始化环境
source build/envsetup.sh
breakfast kirby

# 开始构建（使用所有 CPU 核心）
brunch kirby

# 或者限制并行任务数（如果内存不足或想减少系统负载）
# brunch kirby -j4  # 使用 4 个并行任务
# brunch kirby -j2  # 使用 2 个并行任务（内存 < 16GB 时推荐）
```

**构建时间参考**:
- 高性能机器（32GB+ RAM, 16+ 核心）: 2-3 小时
- 中等性能机器（16GB RAM, 8 核心）: 4-6 小时
- 低性能机器（8GB RAM, 4 核心）: 8+ 小时（不推荐）

**构建过程中的提示**:
- 首次构建会下载更多依赖，时间会更长
- 可以使用 `ccache` 加速后续构建（已自动启用）
- 如果构建失败，查看错误信息并修复后重新构建

### 构建输出

构建完成后，ROM 文件位于：
```
~/lineageos/out/target/product/kirby/
├── lineage-21.0-YYYYMMDD-UNOFFICIAL-kirby.zip  # 刷机包
├── boot.img                                      # Boot 镜像
├── recovery.img                                  # Recovery 镜像
└── ...
```

## ⚡ 性能优化（可选）

### 配置 ccache

ccache 可以显著加速后续构建：

```bash
# 设置 ccache 大小（推荐 50-100GB）
export USE_CCACHE=1
export CCACHE_EXEC=/usr/bin/ccache
ccache -M 50G  # 设置缓存大小为 50GB

# 添加到 ~/.bashrc 使其永久生效
echo 'export USE_CCACHE=1' >> ~/.bashrc
echo 'export CCACHE_EXEC=/usr/bin/ccache' >> ~/.bashrc
echo 'ccache -M 50G' >> ~/.bashrc
```

### 配置交换空间（如果内存不足）

```bash
# 创建 16GB 交换文件（根据内存情况调整）
sudo fallocate -l 16G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile

# 永久启用（添加到 /etc/fstab）
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab
```

### 优化文件系统

如果使用 ext4 文件系统，可以优化挂载选项：

```bash
# 编辑 /etc/fstab，为构建目录所在分区添加 noatime 选项
# 例如：UUID=xxx /home ext4 defaults,noatime 0 2
```

## 🔧 故障排除

### 问题 1: 内存不足

如果构建时出现内存不足错误：

```bash
# 减少并行任务数
brunch kirby -j2  # 使用 2 个并行任务

# 或者增加交换空间
sudo fallocate -l 8G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

### 问题 2: Java 版本错误

如果遇到 Java 版本问题：

```bash
# 检查当前 Java 版本
java -version

# 如果版本不对，更新 JAVA_HOME
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export PATH=$JAVA_HOME/bin:$PATH
```

### 问题 3: 网络问题导致同步失败

```bash
# 使用更少的并发任务
repo sync -c -j2

# 或者使用镜像源（如果有）
# 编辑 .repo/manifests/default.xml，替换为镜像源
```

### 问题 4: Git LFS 文件下载失败

```bash
# 确保 Git LFS 已安装
git lfs install

# 手动拉取 LFS 文件
git lfs pull

# 如果 LFS 文件下载很慢，可以配置代理
git config --global http.proxy http://proxy.example.com:8080
```

### 问题 5: breakfast 命令失败

如果 `breakfast kirby` 失败，检查：

```bash
# 1. 检查设备树是否存在
ls -la device/lenovo/kirby/BoardConfig.mk

# 2. 检查内核是否存在
ls -la kernel/lenovo/kirby/kernel/arch/arm64/configs/kirby_defconfig

# 3. 检查 vendor 文件是否存在
ls -la vendor/lenovo/kirby/

# 4. 手动设置环境变量
export TARGET_DEVICE=kirby
export TARGET_PRODUCT=lineage_kirby
export TARGET_BUILD_VARIANT=userdebug
```

### 问题 6: 构建过程中断

如果构建过程中断：

```bash
# 继续构建（不会重新编译已完成的部分）
cd ~/lineageos
source build/envsetup.sh
breakfast kirby
mka bacon  # 或 brunch kirby
```

## 📦 项目结构

```
Y700-Gen3-Droidian/
├── device/lenovo/kirby/          # 设备树
│   ├── BoardConfig.mk            # 板级配置
│   ├── device.mk                 # 设备配置
│   ├── lineage_kirby.mk          # LineageOS 产品配置
│   ├── extract-files.py          # 专有文件提取脚本
│   └── proprietary-files.txt     # 专有文件列表
├── kernel/lenovo/kirby/          # 内核源码
│   ├── kernel/                   # 内核源码
│   └── prebuilts/                # 预编译工具链
└── vendor/lenovo/kirby/          # 专有文件
    └── [专有文件...]
```

## 🔄 更新源码

```bash
cd ~/lineageos

# 更新 LineageOS 源码
repo sync -c -j$(nproc --all)

# 更新设备树（如果需要）
cd device/lenovo/kirby
git pull

# 更新内核（如果需要）
cd ../../../kernel/lenovo/kirby
git pull
```

## 📝 开发指南

### 提取专有文件

如果你有设备并想更新专有文件：

```bash
cd ~/lineageos/device/lenovo/kirby

# 确保设备已通过 ADB 连接
adb devices

# 生成专有文件列表
./generate-proprietary-files.sh

# 提取专有文件
python3 extract-files.py
```

### 清理构建

```bash
cd ~/lineageos

# 清理构建产物
make clean

# 或者完全清理
rm -rf out/
```

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

本项目遵循 Apache License 2.0。

## 🙏 致谢

- [LineageOS](https://lineageos.org/) - 提供基础系统
- [OnePlus Pad Pro (caihong)](https://wiki.lineageos.org/devices/caihong/) - 参考设备（同平台）
- [Y700 Gen 2 (asphalt)](https://github.com/lolipuru/android_device_lenovo_asphalt) - 参考设备（同品牌）

## 📚 相关链接

- [LineageOS Wiki](https://wiki.lineageos.org/)
- [LineageOS Build Guide](https://wiki.lineageos.org/devices/caihong/build/variant1/)
- [Android Open Source Project](https://source.android.com/)

## ⚠️ 免责声明

- 本 ROM 为**非官方**版本，仅供学习和研究使用
- 刷机有风险，请自行承担风险
- 建议在刷机前备份重要数据
- 作者不对使用本 ROM 造成的任何损失负责

---

**最后更新**: 2024-11-17

