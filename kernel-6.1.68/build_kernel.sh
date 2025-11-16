#! /bin/bash

export PATH=$PWD/prebuilts/build-tools/path/linux-x86:$PATH
export PATH=$PWD/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:$PATH
export PATH=$PWD/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin:$PATH
export ARCH=arm64
export CLANG_TRIPLE=aarch64-linux-gnu-

export CROSS_COMPILE=aarch64-linux-androidkernel-
export LD_LIBRARY_PATH=prebuilts/clang/host/linux-x86/clang-r487747c/lib64:$$LD_LIBRARY_PATH


cd kernel/
rm -rf out/

make ARCH=arm64 CC=$PWD/../prebuilts/clang/host/linux-x86/clang-r487747c/bin/clang  O=out kirby_defconfig

make ARCH=arm64 CROSS_COMPILE=$PWD/../prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-androidkernel- CLANG_TRIPLE=aarch64-linux-gnu-  CC=$PWD/../prebuilts/clang/host/linux-x86/clang-r487747c/bin/clang   O=out
