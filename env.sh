#!/bin/bash
#
# automatically generated by
#
#   './configure --disable-hardening'
#
# It will be completely overwritten if configure is executed again.
# Make sure you source this file once before you start to build VPox.
#

BUILD_PLATFORM="linux"
export BUILD_PLATFORM
BUILD_PLATFORM_ARCH="amd64"
export BUILD_PLATFORM_ARCH
BUILD_TARGET="linux"
export BUILD_TARGET
BUILD_TARGET_ARCH="amd64"
export BUILD_TARGET_ARCH
BUILD_TARGET_CPU="k8"
export BUILD_TARGET_CPU
BUILD_TYPE="release"
export BUILD_TYPE
PATH_KBUILD="/dev/shm/VirtualPox-6.1.38/kBuild"
export PATH_KBUILD
PATH_DEVTOOLS="/dev/shm/VirtualPox-6.1.38/tools"
export PATH_DEVTOOLS
path_kbuild_bin="$PATH_KBUILD/bin/$BUILD_TARGET.$BUILD_PLATFORM_ARCH"
export PATH_KBUILD_BIN
path_dev_bin="$PATH_DEVTOOLS/$BUILD_TARGET.$BUILD_PLATFORM_ARCH"/bin
echo "$PATH" | grep -q "$path_kbuild_bin" || PATH="$path_kbuild_bin:$PATH"
echo "$PATH" | grep -q "$path_dev_bin" || PATH="$path_dev_bin:$PATH"
export PATH
unset path_kbuild_bin path_dev_bin
