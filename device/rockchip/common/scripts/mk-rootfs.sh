#!/bin/bash -e

build_buildroot()
{
	check_config RK_BUILDROOT_CFG || return 0

	ROOTFS_DIR="${1:-$RK_OUTDIR/buildroot}"

	/usr/bin/time -f "you take %E to build buildroot" \
		"$SCRIPTS_DIR/mk-buildroot.sh" $RK_BUILDROOT_CFG "$ROOTFS_DIR"

	cat "$RK_LOG_DIR/post-rootfs.log"
	finish_build build_buildroot $@
}

build_yocto()
{
	check_config RK_YOCTO_CFG RK_KERNEL_VERSION_REAL || return 0

	ROOTFS_DIR="${1:-$RK_OUTDIR/yocto}"

	"$SCRIPTS_DIR/check-yocto.sh"

	cd yocto
	ln -sf $RK_YOCTO_CFG.conf build/conf/local.conf

	{
		echo "PREFERRED_VERSION_linux-rockchip := \"$RK_KERNEL_VERSION_REAL%\""
		echo "LINUXLIBCVERSION := \"$RK_KERNEL_VERSION_REAL-custom%\""
		case "$RK_CHIP_FAMILY" in
			px30|rk3326|rk3562|rk3566_rk3568|rk3588)
				echo "MALI_VERSION := \"g13p0\"" ;;
		esac
		echo "DISPLAY_PLATFORM := \"$RK_YOCTO_DISPLAY_PLATFORM\""
	} > build/rksdk-override.conf

	source oe-init-build-env build
	LANG=en_US.UTF-8 LANGUAGE=en_US.en LC_ALL=en_US.UTF-8 \
		bitbake core-image-minimal -f -c rootfs -c image_complete \
		-R conf/include/rksdk.conf -R rksdk-override.conf

	ln -rsf "$PWD/latest/rootfs.img" $ROOTFS_DIR/rootfs.ext4

	touch "$RK_LOG_DIR/post-rootfs.log"
	cat "$RK_LOG_DIR/post-rootfs.log"
	finish_build build_yocto $@
}

build_debian()
{
	check_config RK_DEBIAN_VERSION || return 0

	ROOTFS_DIR="${1:-$RK_OUTDIR/debian}"
	ARCH=${RK_DEBIAN_ARCH:-armhf}

	"$SCRIPTS_DIR/check-debian.sh"

	cd debian
	if [ ! -f linaro-$RK_DEBIAN_VERSION-alip-*.tar.gz ]; then
		RELEASE=$RK_DEBIAN_VERSION TARGET=desktop ARCH=$ARCH \
			./mk-base-debian.sh
		ln -sf linaro-$RK_DEBIAN_VERSION-alip-*.tar.gz \
			linaro-$RK_DEBIAN_VERSION-$ARCH.tar.gz
	fi

	VERSION=debug ARCH=$ARCH ./mk-rootfs-$RK_DEBIAN_VERSION.sh
	./mk-image.sh

	ln -rsf "$PWD/linaro-rootfs.img" $ROOTFS_DIR/rootfs.ext4

	finish_build build_debian $@
}

# Hooks

usage_hook()
{
	echo -e "buildroot-config[:<config>]       \tmodify buildroot defconfig"
	echo -e "rootfs[:<rootfs type>]            \tbuild default rootfs"
	echo -e "buildroot                         \tbuild buildroot rootfs"
	echo -e "yocto                             \tbuild yocto rootfs"
	echo -e "debian                            \tbuild debian rootfs"
}

clean_hook()
{
	rm -rf yocto/build/tmp yocto/build/*cache
	rm -rf debian/binary

	if check_config RK_BUILDROOT_CFG &>/dev/null; then
		rm -rf buildroot/output/$RK_BUILDROOT_CFG
	fi

	rm -rf "$RK_OUTDIR/buildroot"
	rm -rf "$RK_OUTDIR/yocto"
	rm -rf "$RK_OUTDIR/debian"
	rm -rf "$RK_OUTDIR/rootfs"
}

PRE_BUILD_CMDS="buildroot-config"
pre_build_hook()
{
	BUILDROOT_BOARD="${2:-"$RK_BUILDROOT_CFG"}"

	[ "$BUILDROOT_BOARD" ] || return 0

	TEMP_DIR=$(mktemp -d)
	"$SDK_DIR/buildroot/build/parse_defconfig.sh" "$BUILDROOT_BOARD" \
		"$TEMP_DIR/.config"
	make -C "$SDK_DIR/buildroot" O="$TEMP_DIR" menuconfig
	"$SDK_DIR/buildroot/build/update_defconfig.sh" "$BUILDROOT_BOARD" \
		"$TEMP_DIR"

	finish_build $@
}

BUILD_CMDS="rootfs buildroot debian yocto"
build_hook()
{
	check_config RK_ROOTFS_TYPE || return 0

	if [ -z "$1" -o "$1" = rootfs ]; then
		ROOTFS=${RK_ROOTFS_SYSTEM:-buildroot}
	else
		ROOTFS=$1
	fi

	ROOTFS_IMG=rootfs.${RK_ROOTFS_TYPE}
	ROOTFS_DIR="$RK_OUTDIR/rootfs"

	echo "=========================================="
	echo "          Start building rootfs($ROOTFS)"
	echo "=========================================="

	rm -rf "$ROOTFS_DIR"
	mkdir -p "$ROOTFS_DIR"

	case "$ROOTFS" in
		yocto) build_yocto "$ROOTFS_DIR" ;;
		debian) build_debian "$ROOTFS_DIR" ;;
		buildroot) build_buildroot "$ROOTFS_DIR" ;;
		*) usage ;;
	esac

	if [ ! -f "$ROOTFS_DIR/$ROOTFS_IMG" ]; then
		echo "There's no $ROOTFS_IMG generated..."
		exit 1
	fi

	ln -rsf "$ROOTFS_DIR/$ROOTFS_IMG" "$RK_FIRMWARE_DIR/rootfs.img"

	# For builtin OEM image
	[ ! -e "$ROOTFS_DIR/oem.img" ] || \
		ln -rsf "$ROOTFS_DIR/oem.img" "$RK_FIRMWARE_DIR"

	if [ "$RK_ROOTFS_INITRD" ]; then
		/usr/bin/time -f "you take %E to pack ramboot image" \
			"$SCRIPTS_DIR/mk-ramdisk.sh" \
			"$RK_FIRMWARE_DIR/rootfs.img" \
			"$ROOTFS_DIR/ramboot.img" "$RK_BOOT_FIT_ITS"
		ln -rsf "$ROOTFS_DIR/ramboot.img" \
			"$RK_FIRMWARE_DIR/boot.img"

		# For security
		cp "$RK_FIRMWARE_DIR/boot.img" u-boot/
	fi

	if [ "$RK_SECURITY" ]; then
		echo "Try to build init for $RK_SECURITY_CHECK_METHOD"

		if [ "$RK_SECURITY_CHECK_METHOD" = "DM-V" ]; then
			SYSTEM_IMG=rootfs.squashfs
		else
			SYSTEM_IMG=$ROOTFS_IMG
		fi
		if [ ! -f "$ROOTFS_DIR/$SYSTEM_IMG" ]; then
			echo "There's no $SYSTEM_IMG generated..."
			exit -1
		fi

		"$SCRIPTS_DIR/mk-dm.sh" $RK_SECURITY_CHECK_METHOD \
			"$ROOTFS_DIR/$SYSTEM_IMG"
		ln -rsf "$ROOTFS_DIR/security-system.img" \
			"$RK_FIRMWARE_DIR/rootfs.img"
	fi

	finish_build build_rootfs $@
}

source "${BUILD_HELPER:-$(dirname "$(realpath "$0")")/../build-hooks/build-helper}"

case "${1:-rootfs}" in
	buildroot-config) pre_build_hook $@ ;;
	*) build_hook $@ ;;
esac
