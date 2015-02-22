#!/bin/bash

# Colorize and add text parameters
export red=$(tput setaf 1)             #  red
export grn=$(tput setaf 2)             #  green
export blu=$(tput setaf 4)             #  blue
export cya=$(tput setaf 6)             #  cyan
export txtbld=$(tput bold)             #  Bold
export bldred=${txtbld}$(tput setaf 1) #  red
export bldgrn=${txtbld}$(tput setaf 2) #  green
export bldblu=${txtbld}$(tput setaf 4) #  blue
export bldcya=${txtbld}$(tput setaf 6) #  cyan
export txtrst=$(tput sgr0)             #  Reset

# location
if [ "${1}" != "" ]; then
	export KERNELDIR=`readlink -f ${1}`;
else
	export KERNELDIR=`readlink -f .`;
fi;

export PARENT_DIR=`readlink -f ${KERNELDIR}/..`;
export INITRAMFS_SOURCE=`readlink -f $PARENT_DIR/../ramdisks/shamu`;
export INITRAMFS_TMP=${KERNELDIR}/tmp/initramfs_source;

# check if parallel installed, if not install
if [ ! -e /usr/bin/parallel ]; then
	echo "You must install 'parallel' to continue.";
	sudo apt-get install parallel
fi

# check if ccache installed, if not install
if [ ! -e /usr/bin/ccache ]; then
	echo "You must install 'ccache' to continue.";
	sudo apt-get install ccache
fi

# check if adb installed, if not install
if [ ! -e /usr/bin/adb ]; then
	echo "You must install 'adb' to continue.";
	sudo apt-get install android-tools-adb
fi

# check if xmllint installed, if not install
if [ ! -e /usr/bin/xmllint ]; then
	echo "You must install 'xmllint' to continue.";
	sudo apt-get install libxml2-utils
fi

# kernel
export ARCH=arm;
export SUB_ARCH=arm;
export KERNEL_CONFIG="hydra_shamu_defconfig";

# build script
export USER=`whoami`;
export TMPFILE=`mktemp -t`;
export KBUILD_BUILD_USER="NeoBuddy89";
export KBUILD_BUILD_HOST="DragonCore";

# system compiler
# export CROSS_COMPILE=$PARENT_DIR/../toolchains/linaro-toolchain-4.8-2013.12/bin/arm-eabi-;
# export CROSS_COMPILE=$PARENT_DIR/../toolchains/linaro-toolchain-4.7-2013.12/bin/arm-eabi-;
# export CROSS_COMPILE=$PARENT_DIR/../toolchains/arm-eabi-4.8/bin/arm-eabi-;
# export CROSS_COMPILE=$PARENT_DIR/../toolchains/arm-eabi-4.7/bin/arm-eabi-;

# Use hammerhead optimized toolchain!
# export CROSS_COMPILE=$PARENT_DIR/../toolchains/arm-hammerhead-linux-gnueabi-4.9.3/bin/arm-hammerhead-linux-gnueabi-;

export CROSS_COMPILE=$PARENT_DIR/../toolchains/arm-cortex_a15-linux-gnueabihf-linaro_4.9.3-2015.02/bin/arm-cortex_a15-linux-gnueabihf-;

if [ ! -f ${CROSS_COMPILE}gcc ]; then
	echo "${bldred}Cannot find GCC compiler ${CROSS_COMPILE}gcc${txtrst}";
	echo "${bldcya}Please ensure you have GCC Compiler at path mentioned in env_setup.sh and then you can continue.${txtrst}";
	exit 1;
fi

if [ ! -f ${INITRAMFS_SOURCE}/init ]; then
	echo "${bldred}Cannot find proper ramdisk at ${INITRAMFS_SOURCE}${txtrst}";
	echo "${bldcya}Please ensure you have RAMDISK at path mentioned in env_setup.sh and then you can continue.${txtrst}";
	exit 1;
fi

export NUMBEROFCPUS=`grep 'processor' /proc/cpuinfo | wc -l`;
