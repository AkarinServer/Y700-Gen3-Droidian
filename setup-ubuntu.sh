#!/bin/bash
#
# Ubuntu 22.04 环境配置脚本
# 用于在全新的 Ubuntu 22.04 系统上配置 LineageOS 构建环境
#

set -e

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  LineageOS 构建环境配置脚本                                  ║"
echo "║  适用于 Ubuntu 22.04                                         ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# 检查是否为 root 用户
if [ "$EUID" -eq 0 ]; then 
   echo "❌ 请不要使用 root 用户运行此脚本"
   exit 1
fi

# 步骤 1: 更新系统
echo "📦 步骤 1/8: 更新系统包列表..."
sudo apt-get update

# 步骤 2: 安装系统依赖
echo ""
echo "📦 步骤 2/8: 安装构建依赖..."
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
    zlib1g-dev \
    python3 \
    python3-pip \
    git-lfs \
    openjdk-17-jdk \
    android-tools-adb \
    android-tools-fastboot

# 步骤 3: 配置 Java
echo ""
echo "☕ 步骤 3/8: 配置 Java 17 环境..."
JAVA_HOME_PATH="/usr/lib/jvm/java-17-openjdk-amd64"
if [ -d "$JAVA_HOME_PATH" ]; then
    echo "export JAVA_HOME=$JAVA_HOME_PATH" >> ~/.bashrc
    echo "export PATH=\$JAVA_HOME/bin:\$PATH" >> ~/.bashrc
    export JAVA_HOME=$JAVA_HOME_PATH
    export PATH=$JAVA_HOME/bin:$PATH
    echo "✅ Java 环境已配置"
    java -version
else
    echo "⚠️  警告: Java 17 未找到，请手动安装"
fi

# 步骤 4: 安装 Repo 工具
echo ""
echo "📥 步骤 4/8: 安装 Repo 工具..."
mkdir -p ~/bin
if [ ! -f ~/bin/repo ]; then
    curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
    chmod a+x ~/bin/repo
    echo "✅ Repo 工具已安装"
else
    echo "✅ Repo 工具已存在"
fi

# 添加到 PATH
if ! grep -q "~/bin" ~/.bashrc; then
    echo "" >> ~/.bashrc
    echo "# Repo tool" >> ~/.bashrc
    echo 'export PATH=~/bin:$PATH' >> ~/.bashrc
fi
export PATH=~/bin:$PATH

# 验证 Repo
if command -v repo &> /dev/null; then
    echo "✅ Repo 版本: $(repo --version | head -1)"
else
    echo "⚠️  警告: Repo 工具未正确安装"
fi

# 步骤 5: 配置 Git
echo ""
echo "🔧 步骤 5/8: 配置 Git 和 Git LFS..."

# 检查 Git 用户配置
if [ -z "$(git config --global user.name)" ]; then
    echo "⚠️  Git 用户信息未配置，请手动运行："
    echo "   git config --global user.name \"Your Name\""
    echo "   git config --global user.email \"your.email@example.com\""
fi

# 初始化 Git LFS
if command -v git-lfs &> /dev/null; then
    git lfs install
    echo "✅ Git LFS 已初始化"
else
    echo "⚠️  警告: Git LFS 未安装"
fi

# 配置 Git 大文件处理
git config --global core.bigFileThreshold 1
git config --global http.postBuffer 524288000
git config --global http.lowSpeedLimit 0
git config --global http.lowSpeedTime 999999
echo "✅ Git 配置已优化"

# 步骤 6: 配置 ccache
echo ""
echo "⚡ 步骤 6/8: 配置 ccache..."
if command -v ccache &> /dev/null; then
    if ! grep -q "USE_CCACHE" ~/.bashrc; then
        echo "" >> ~/.bashrc
        echo "# ccache configuration" >> ~/.bashrc
        echo "export USE_CCACHE=1" >> ~/.bashrc
        echo "export CCACHE_EXEC=/usr/bin/ccache" >> ~/.bashrc
    fi
    export USE_CCACHE=1
    export CCACHE_EXEC=/usr/bin/ccache
    ccache -M 50G
    echo "✅ ccache 已配置（缓存大小: 50GB）"
else
    echo "⚠️  警告: ccache 未安装"
fi

# 步骤 7: 检查磁盘空间
echo ""
echo "💾 步骤 7/8: 检查磁盘空间..."
AVAILABLE_SPACE=$(df -BG ~ | tail -1 | awk '{print $4}' | sed 's/G//')
echo "可用空间: ${AVAILABLE_SPACE}GB"

if [ "$AVAILABLE_SPACE" -lt 250 ]; then
    echo "⚠️  警告: 可用空间不足 250GB，建议至少 250GB 用于构建"
else
    echo "✅ 磁盘空间充足"
fi

# 步骤 8: 验证安装
echo ""
echo "✅ 步骤 8/8: 验证安装..."
echo ""

echo "检查已安装的工具："
echo -n "  Git: "
git --version | head -1 || echo "❌ 未安装"

echo -n "  Python: "
python3 --version || echo "❌ 未安装"

echo -n "  Java: "
java -version 2>&1 | head -1 || echo "❌ 未安装"

echo -n "  Repo: "
repo --version 2>&1 | head -1 || echo "❌ 未安装"

echo -n "  Git LFS: "
git lfs version | head -1 || echo "❌ 未安装"

echo -n "  ccache: "
ccache --version | head -1 || echo "❌ 未安装"

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  ✅ 环境配置完成！                                           ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "📝 下一步："
echo "  1. 重新打开终端或运行: source ~/.bashrc"
echo "  2. 配置 Git 用户信息（如果未配置）:"
echo "     git config --global user.name \"Your Name\""
echo "     git config --global user.email \"your.email@example.com\""
echo "  3. 按照 README.md 中的步骤继续："
echo "     - 初始化 LineageOS 源码"
echo "     - 克隆设备树和内核"
echo "     - 开始构建"
echo ""
echo "💡 提示: 如果内存不足 16GB，建议创建交换空间"
echo "   运行: sudo fallocate -l 16G /swapfile && sudo chmod 600 /swapfile && sudo mkswap /swapfile && sudo swapon /swapfile"
echo ""

