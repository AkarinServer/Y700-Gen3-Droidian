#!/bin/bash
#
# 从设备生成专有文件列表
#

export PATH=$HOME/platform-tools:$PATH

echo "正在从设备生成专有文件列表..."
echo ""

# 创建临时文件
TEMP_DIR=$(mktemp -d)
VENDOR_LIBS="$TEMP_DIR/vendor_libs.txt"
VENDOR_ETC="$TEMP_DIR/vendor_etc.txt"
VENDOR_BIN="$TEMP_DIR/vendor_bin.txt"
VENDOR_FRAMEWORK="$TEMP_DIR/vendor_framework.txt"

# 获取 vendor/lib64 下的 .so 文件
echo "获取 vendor/lib64 库文件..."
adb shell "find /vendor/lib64 -name '*.so' -type f 2>/dev/null" | sed 's|^/vendor/||' | sort > "$VENDOR_LIBS"

# 获取 vendor/etc 下的配置文件
echo "获取 vendor/etc 配置文件..."
adb shell "find /vendor/etc -type f \( -name '*.xml' -o -name '*.conf' -o -name '*.json' -o -name '*.txt' \) 2>/dev/null" | sed 's|^/vendor/||' | sort > "$VENDOR_ETC"

# 获取 vendor/bin 下的可执行文件
echo "获取 vendor/bin 可执行文件..."
adb shell "find /vendor/bin -type f -executable 2>/dev/null" | sed 's|^/vendor/||' | sort > "$VENDOR_BIN"

# 获取 vendor/framework 下的 jar 文件
echo "获取 vendor/framework 框架文件..."
adb shell "find /vendor/framework -name '*.jar' -type f 2>/dev/null" | sed 's|^/vendor/||' | sort > "$VENDOR_FRAMEWORK"

# 合并并生成 proprietary-files.txt
OUTPUT_FILE="proprietary-files.txt.new"
echo "# 专有文件列表 - 从设备自动生成" > "$OUTPUT_FILE"
echo "# 生成时间: $(date)" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "# Vendor 库文件" >> "$OUTPUT_FILE"
cat "$VENDOR_LIBS" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "# Vendor 配置文件" >> "$OUTPUT_FILE"
cat "$VENDOR_ETC" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "# Vendor 可执行文件" >> "$OUTPUT_FILE"
cat "$VENDOR_BIN" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo "# Vendor 框架文件" >> "$OUTPUT_FILE"
cat "$VENDOR_FRAMEWORK" >> "$OUTPUT_FILE"

# 统计
TOTAL=$(cat "$VENDOR_LIBS" "$VENDOR_ETC" "$VENDOR_BIN" "$VENDOR_FRAMEWORK" | wc -l)
echo ""
echo "✅ 生成完成！"
echo "   总文件数: $TOTAL"
echo "   输出文件: $OUTPUT_FILE"
echo ""
echo "文件统计："
echo "  - 库文件 (.so): $(wc -l < "$VENDOR_LIBS")"
echo "  - 配置文件: $(wc -l < "$VENDOR_ETC")"
echo "  - 可执行文件: $(wc -l < "$VENDOR_BIN")"
echo "  - 框架文件 (.jar): $(wc -l < "$VENDOR_FRAMEWORK")"

# 清理
rm -rf "$TEMP_DIR"

echo ""
echo "请检查 $OUTPUT_FILE，确认无误后替换原有的 proprietary-files.txt"

