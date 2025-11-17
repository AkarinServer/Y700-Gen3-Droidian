#!/bin/bash
#
# WSL2 ADB 设置和连接脚本
# 用于在 WSL2 中访问 Windows 下的 ADB 设备
#

set -e

echo "=========================================="
echo "WSL2 ADB 设置脚本"
echo "=========================================="

# 获取 Windows 主机 IP
WINDOWS_IP=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}' 2>/dev/null || echo "")
if [ -z "$WINDOWS_IP" ]; then
    WINDOWS_IP=$(ip route show | grep default | awk '{print $3}' | head -1)
fi

echo "检测到 Windows 主机 IP: $WINDOWS_IP"

# 检查 adb 是否已安装
if ! command -v adb &> /dev/null; then
    echo ""
    echo "⚠️  adb 未安装，正在安装..."
    sudo apt update
    sudo apt install -y android-tools-adb android-tools-fastboot
    echo "✅ adb 安装完成"
else
    echo "✅ adb 已安装: $(adb version | head -1)"
fi

echo ""
echo "=========================================="
echo "连接选项："
echo "=========================================="
echo "1. 连接到 Windows 上的 ADB Server (端口 5037)"
echo "2. 通过网络 ADB 连接设备 (端口 5555)"
echo "3. 仅检查当前连接"
echo ""
read -p "请选择 (1/2/3): " choice

case $choice in
    1)
        echo ""
        echo "尝试连接到 Windows ADB Server ($WINDOWS_IP:5037)..."
        export ADB_SERVER_SOCKET=tcp:$WINDOWS_IP:5037
        
        # 尝试连接
        if timeout 2 bash -c "echo > /dev/tcp/$WINDOWS_IP/5037" 2>/dev/null; then
            echo "✅ Windows ADB Server 可访问"
            adb connect $WINDOWS_IP:5037 2>/dev/null || true
        else
            echo "❌ 无法连接到 Windows ADB Server"
            echo "   请确保："
            echo "   1. 在 Windows 上运行 'adb start-server'"
            echo "   2. Windows 防火墙允许端口 5037"
        fi
        ;;
    2)
        echo ""
        read -p "请输入设备 IP 地址 (例如: 192.168.1.100): " device_ip
        if [ -n "$device_ip" ]; then
            echo "连接到设备 $device_ip:5555..."
            adb connect $device_ip:5555
        else
            echo "❌ 未输入 IP 地址"
            exit 1
        fi
        ;;
    3)
        echo ""
        echo "检查当前连接..."
        ;;
    *)
        echo "❌ 无效选择"
        exit 1
        ;;
esac

echo ""
echo "=========================================="
echo "当前连接的设备："
echo "=========================================="
adb devices -l

echo ""
echo "=========================================="
echo "设备信息："
echo "=========================================="
if adb devices | grep -q "device$"; then
    echo "设备型号: $(adb shell getprop ro.product.model 2>/dev/null || echo 'N/A')"
    echo "设备代号: $(adb shell getprop ro.product.device 2>/dev/null || echo 'N/A')"
    echo "Android 版本: $(adb shell getprop ro.build.version.release 2>/dev/null || echo 'N/A')"
    echo "构建指纹: $(adb shell getprop ro.build.fingerprint 2>/dev/null || echo 'N/A')"
else
    echo "⚠️  未检测到设备"
    echo ""
    echo "请尝试："
    echo "1. 在 Windows 上运行 'adb devices' 确认设备连接"
    echo "2. 在设备上确认 USB 调试授权"
    echo "3. 使用网络 ADB: adb tcpip 5555 (在 Windows 上通过 USB 连接时)"
fi

echo ""
echo "=========================================="
echo "完成！"
echo "=========================================="

