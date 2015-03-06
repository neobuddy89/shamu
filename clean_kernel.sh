#!/bin/bash

echo "***** Setting up Environment *****";

. ./env_setup.sh ${1} || exit 1;

echo "${bldcya}***** Cleaning in Progress *****${txtrst}";

# clean up kernel compiled binaries
make mrproper;
make clean;

# clean up generated files
rm -rf $INITRAMFS_TMP >> /dev/null;
rm -f $KERNELDIR/arch/arm/boot/*.dtb >> /dev/null;
rm -f $KERNELDIR/arch/arm/boot/*.cmd >> /dev/null;
rm -f $KERNELDIR/arch/arm/boot/zImage-dtb >> /dev/null;
rm -f $KERNELDIR/arch/arm/mach-msm/smd_rpc_sym.c >> /dev/null;
rm -f $KERNELDIR/arch/arm/crypto/aesbs-core.S >> /dev/null;
rm -f $KERNELDIR/r*.cpio >> /dev/null;
rm -f $KERNELDIR/ramdisk.gz >> /dev/null;
rm -rf $KERNELDIR/include/generated >> /dev/null;
rm -rf $KERNELDIR/arch/*/include/generated >> /dev/null;
rm -f $KERNELDIR/zImage >> /dev/null;
rm -f $KERNELDIR/out/zImage >> /dev/null;
rm -f $KERNELDIR/zImage-dtb >> /dev/null;
rm -f $KERNELDIR/out/boot.img >> /dev/null;
rm -f $KERNELDIR/boot.img >> /dev/null;
rm -f $KERNELDIR/dt.img >> /dev/null;
rm -rf $KERNELDIR/out/system/lib/modules >> /dev/null;
rm -rf $KERNELDIR/out/tmp_modules >> /dev/null;
rm -f $KERNELDIR/out/Hydra_* >> /dev/null;
rm -rf $KERNELDIR/tmp >> /dev/null;

# clean up leftover junk
find . -type f \( -iname \*.rej \
				-o -iname \*.orig \
				-o -iname \*.bkp \
				-o -iname \*.ko \
				-o -iname \*.c.BACKUP.[0-9]*.c \
				-o -iname \*.c.BASE.[0-9]*.c \
				-o -iname \*.c.LOCAL.[0-9]*.c \
				-o -iname \*.c.REMOTE.[0-9]*.c \
				-o -iname \*.org \) \
					| parallel rm -fv {};

echo "${bldcya}***** Cleaning Done *****${txtrst}";


