#!/bin/bash

TOP=`pwd`
UBOOT_VER=2014.07
KERNEL_VER=3.4.39
BUILDROOT_VER=2013.11

if [ $# -ge 1 ]; then
	BOARD_NAME=$1
	BOOT_DEV=$2
	CHIPSET_NAME=s5p6818
else
	echo "Please specify your board name."
	echo "Usage : ./platform/"${CHIPSET_NAME}"/tools/build.sh [BOARD_NAME] [BOOT_DEV]"
	echo "Supported board : drone/svt"
	echo "Avaliable boot device : sdmmc/spi"
	exit 0
fi

if [ $1 == "drone" ]; then
	echo ""
else
	if [ $1 == "svt" ]; then
		echo ""
	else
		echo "Not supported board!"
		echo "Supported board : drone/svt"
		exit 0
	fi
fi

if [ $2 == "sdmmc" ]; then
    echo ""
else
    if [ $2 == "spi" ]; then
        echo ""
    else
        echo "Not supported boot device!"
        echo "Avaliable boot device : sdmmc/spi"
        exit 0
    fi
fi

#LOAD_ADDRESS=0x43c00000
#LAUNCH_ADDRESS=0x43c00000

UBOOT_CONFIG_NAME=${CHIPSET_NAME}_${BOARD_NAME}
KERNEL_CONFIG_NAME=${CHIPSET_NAME}_${BOARD_NAME}
 
UBOOT_DIR=$TOP/bootloader/u-boot-${UBOOT_VER}
KERNEL_DIR=$TOP/kernel/kernel-${KERNEL_VER}

MODULES_DIR=$TOP/platform/${CHIPSET_NAME}/modules
APPLICATION_DIR=$TOP/platform/${CHIPSET_NAME}/apps
LIBRARY_DIR=$TOP/platform/${CHIPSET_NAME}/library

FILESYSTEM_DIR=$TOP/platform/common/fs
BUILDROOT_DIR=$FILESYSTEM_DIR/buildroot/buildroot-${BUILDROOT_VER}
TOOLS_DIR=$TOP/platform/common/tools
RESULT_DIR=$TOP/platform/${CHIPSET_NAME}/result

# Kbyte default:11,264, 16384, 24576, 32768, 49152, 
RAMDISK_SIZE=32768
RAMDISK_FILE=$FILESYSTEM_DIR/buildroot/out/ramdisk.gz

NX_BINGEN=$TOOLS_DIR/bin/BOOT_BINGEN
NSIH_FILE=$TOP/platform/${CHIPSET_NAME}/boot/release/nsih/nsih_${BOARD_NAME}_${BOOT_DEV}.txt
SECONDBOOT_FILE=$TOP/platform/${CHIPSET_NAME}/boot/release/2ndboot/2ndboot_${BOARD_NAME}_${BOOT_DEV}.bin
SECONDBOOT_OUT_FILE=$RESULT_DIR/2ndboot_${BOARD_NAME}.bin

CMD_V_BUILD_NUM=

CMD_V_2NDBOOT=no
CMD_V_UBOOT=no
CMD_V_UBOOT_CLEAN=no

CMD_V_KERNEL=no
CMD_V_KERNEL_CLEAN=no
CMD_V_KERNEL_MODULE=no

CMD_V_KERNEL_PROJECT_MENUCONFIG=no
CMD_V_KERNEL_PROJECT_MENUCONFIG_COMPILE=no

CMD_V_APPLICATION=no
CMD_V_APPLICATION_CLEAN=no
CMD_V_BUILDROOT=no
CMD_V_BUILDROOT_CLEAN=no
CMD_V_FILESYSTEM=no

CMD_V_SDCARD_PACKAGING=no
CMD_V_SDCARD_SELECT_DEV=
CMD_V_EMMC_PACKAGING=no
CMD_V_EMMC_PACKAGING_2NDBOOT=no
CMD_V_EMMC_PACKAGING_UBOOT=no
CMD_V_EMMC_PACKAGING_BOOT=no

CMD_V_BASE_PORTING=no
CMD_V_NEW_BOARD=

CMD_V_BUILD_ERROR=no
CMD_V_BUILD_SEL=Not

TEMP_UBOOT_TEXT=
TEMP_KERNEL_TEXT=

YEAR=0000
MON=00
DAY=00
HOUR=00
MIN=00
SEC=00

function check_result()
{
	if [ $? -ne 0 ]; then
		cd $TOP
		echo "[Error]"
		exit
	fi
}

function currentTime()
{
	YEAR=`date +%Y`
	MON=`date +%m`
	DAY=`date +%d`
	HOUR=`date +%H`
	MIN=`date +%M`
	SEC=`date +%S`
}

function build_2ndboot()
{
    echo ''
    echo ''
    echo '#########################################################'
    echo '#########################################################'
    echo '#'
    echo "# Make 2ndboot ($BOOT_DEV)"
    echo '#'
    echo '#########################################################'
    echo '#########################################################'

    sleep 1.5
    pushd . > /dev/null
	echo "$NX_BINGEN -c $CHIPSET_NAME -t 2ndboot -n $NSIH_FILE -i $SECONDBOOT_FILE -o $SECONDBOOT_OUT_FILE"
#	$NX_BINGEN -c $CHIPSET_NAME -t 2ndboot -n $NSIH_FILE -i $SECONDBOOT_FILE -o $SECONDBOOT_OUT_FILE -l $LOAD_ADDRESS -e $LAUNCH_ADDRESS
	$NX_BINGEN -c $CHIPSET_NAME -t 2ndboot -n $NSIH_FILE -i $SECONDBOOT_FILE -o $SECONDBOOT_OUT_FILE
    popd > /dev/null
}

function build_uboot_source()
{

	if [ ${CMD_V_UBOOT_CLEAN} = "yes" ]; then
		echo ''
		echo ''
		echo '#########################################################'
		echo '#########################################################'
		echo '#'
		echo '# Clean u-boot '
		echo '#'
		echo '#########################################################'

		sleep 1.5

		pushd . > /dev/null
		cd $UBOOT_DIR
		make distclean
		make ${UBOOT_CONFIG_NAME}_config
		popd > /dev/null
	fi

	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo "# Build u-boot "
	echo '#'
	echo '#########################################################'

	if [ -f $RESULT_DIR/build.${CHIPSET_NAME}.uboot ]; then
		rm -f $RESULT_DIR/build.${CHIPSET_NAME}.uboot
	fi
	echo "${UBOOT_CONFIG_NAME}_config" > $RESULT_DIR/build.${CHIPSET_NAME}.uboot

	sleep 1.5
	pushd . > /dev/null

	cd $UBOOT_DIR
	make -j8 -sw
	check_result
	popd > /dev/null
	
	cp -v ${UBOOT_DIR}/u-boot.bin ${RESULT_DIR}
}

function build_kernel_source()
{
	if [ ${CMD_V_KERNEL_CLEAN} = "yes" ]; then
		echo ''
		echo ''
		echo '#########################################################'
		echo '#########################################################'
		echo '#'
		echo '# Clean kernel '
		echo '#'
		echo '#########################################################'

		sleep 1.5

		pushd . > /dev/null
		cd $KERNEL_DIR
		make ARCH=arm distclean -j8
		popd > /dev/null
	fi

	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo "# Build kernel "
	echo '#'
	echo '#########################################################'

	sleep 1.5

    if [ -f $RESULT_DIR/build.${CHIPSET_NAME}.kernel ]; then
	    rm -f $RESULT_DIR/build.${CHIPSET_NAME}.kernel
    fi

	pushd . > /dev/null
	cd $KERNEL_DIR
	make ARCH=arm ${KERNEL_CONFIG_NAME}_linux_defconfig
	echo "${KERNEL_CONFIG_NAME}_linux_defconfig" > $RESULT_DIR/build.${CHIPSET_NAME}.kernel

	make ARCH=arm uImage -j8 -sw
	check_result
	popd > /dev/null

	cp -v ${KERNEL_DIR}/arch/arm/boot/uImage ${RESULT_DIR}
}

function build_kernel_module()
{

	echo ''
	echo '#########################################################'
	echo '# Build kernel modules'
	echo '#########################################################'

	sleep 1.5

	pushd . > /dev/null
	cd $MODULES_DIR/coda960
	make ARCH=arm -j4 -sw
	check_result
	popd > /dev/null
}

function build_kernel_current_menuconfig()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo "# Kernel ${KERNEL_CONFIG_NAME}_linux_defconfig menuconfig "
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	pushd . > /dev/null
	cd $KERNEL_DIR
	make ARCH=arm menuconfig
	check_result
	popd > /dev/null
}

function build_kernel_configcopy()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo "# Kernel set ${KERNEL_CONFIG_NAME}_linux_defconfig "
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	if [ -f $RESULT_DIR/build.${CHIPSET_NAME}.kernel ]; then
		rm -f $RESULT_DIR/build.${CHIPSET_NAME}.kernel
	fi

	sleep 1.5
	pushd . > /dev/null
	cd $KERNEL_DIR
	make distclean
	make ARCH=arm ${KERNEL_CONFIG_NAME}_linux_defconfig
	check_result

	echo "${KERNEL_CONFIG_NAME}_linux_defconfig" > $RESULT_DIR/build.${CHIPSET_NAME}.kernel	
	popd > /dev/null
}

function build_partial_app()
{
	if [ -d $1 ]; then
		echo ''
		echo '#########################################################'
		echo "# $1 "
		echo '#########################################################'
		cd $1
		if [ ${CMD_V_APPLICATION_CLEAN} = "yes" ]; then
			make clean
		fi	
		make -sw
		check_result
	fi
}

function build_partial_lib()
{
	if [ -d $1 ]; then
		echo ''
		echo '#########################################################'
		echo "# $1 "
		echo '#########################################################'
		cd $1
		if [ ${CMD_V_APPLICATION_CLEAN} = "yes" ]; then
			make clean
		fi	
		make -sw
		check_result
		make install -sw
	fi
}

function build_application()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo '# Library & Application '
	echo '#'
	echo '#########################################################'
	echo '#########################################################'
	
	sleep 1.5

	pushd . > /dev/null

	cd $LIBRARY_DIR/src
	make

#	build_partial_app $APPLICATION_DIR/v4l2_test
	build_partial_app $APPLICATION_DIR/vpu_test

	popd > /dev/null
}

function build_buildroot()
{
	cd $BUILDROOT_DIR

	if [ ${CMD_V_BUILDROOT_CLEAN} = "yes" ]; then
        echo '#########################################################'
        echo '#########################################################'
        echo '#'
        echo '# Clean buildroot '
        echo '#'
        echo '#########################################################'
		make clean
	fi

    echo ''
    echo ''
    echo '#########################################################'
    echo '#########################################################'
    echo '#'
    echo "# Build buildroot "
	echo '#'
    echo '#########################################################'
    echo '#########################################################'

	if [ -f .config ]; then
		echo ""
	else
		cp -v ../configs/br.2013.11.cortex_a9_glibc_tiny_rfs.config .config
	fi
	make
    check_result
}

function copy_app()
{
	if [ -d $1 ]; then
		echo '//////////////////////////////////////////////////////////////////////////////'
		echo "// copy $1 "
		cp -v $1/$2 $FILESYSTEM_DIR/buildroot/out/rootfs/usr/bin/
		check_result
	fi
}

function build_filesystem()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo "# filesystem(rootfs-ramdisk)"
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	sleep 1.5

	if [ -f ${RAMDISK_FILE} ]; then
		rm -f ${RAMDISK_FILE}
	fi

	if [ -f ${RESULT_DIR}/ramdisk.gz ]; then
		rm -f ${RESULT_DIR}/ramdisk.gz
	fi

	if [ -d $FILESYSTEM_DIR/buildroot/out/rootfs ]; then
		if [ -d $APPLICATION_DIR/v4l2_test ]; then
			echo '//////////////////////////'
			echo '// copy v4l2_test '
			cd $APPLICATION_DIR/v4l2_test/
			cp -v camera_test csi_test decimator_test hdmi_test $FILESYSTEM_DIR/buildroot/out/rootfs/usr/bin/
			check_result
		fi
		
		if [ -d $APPLICATION_DIR/vpu_test ]; then
			echo '//////////////////////////'
			echo '// copy vpu_test '
			cd $APPLICATION_DIR/vpu_test/
			cp -v dec_test enc_test jpg_test trans_test $FILESYSTEM_DIR/buildroot/out/rootfs/usr/bin/
			check_result
		fi

		echo ''
		echo '//////////////////////////'
		echo '// copy lib '
		cp -v $LIBRARY_DIR/lib/*.so $FILESYSTEM_DIR/buildroot/out/rootfs/usr/lib/
		check_result

		echo ''
		echo '//////////////////////////'
		echo '// copy coda960 '
		cp -v $MODULES_DIR/coda960/nx_vpu.ko $FILESYSTEM_DIR/buildroot/out/rootfs/root/
		check_result
		echo ''

		pushd . > /dev/null
		cd $FILESYSTEM_DIR
		cp buildroot/scripts/mk_ramfs.sh buildroot/out/
		cd buildroot/out/

		if [ -d mnt ]; then
			sudo rm -rf mnt
		fi

		chmod 755 ./*.sh
		./mk_ramfs.sh -r rootfs -s ${RAMDISK_SIZE}

		popd > /dev/null

		echo ''
		echo ''
		echo '#########################################################'
		echo "# copy image"
		echo '#########################################################'
		cp -v ${UBOOT_DIR}/u-boot.bin ${RESULT_DIR}
		check_result
		cp -v ${KERNEL_DIR}/arch/arm/boot/uImage ${RESULT_DIR}
		check_result
		cp -v ${RAMDISK_FILE} ${RESULT_DIR}/ramdisk.gz
		check_result
	else
		echo '##########################################################'
		echo '# error : No "./fs/buildroot/out" folder.'
		echo '# Please build buildroot before making ramdisk filesystem'
		echo '##########################################################'
	fi
}

function build_fastboot_2ndboot()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo '# fastboot 2ndboot'
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	sleep 1.5
	pushd . > /dev/null
	sudo fastboot flash 2ndboot $SECONDBOOT_OUT_FILE
	popd > /dev/null
}

function build_fastboot_uboot()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo '# fastboot uboot'
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	sleep 1.5
	pushd . > /dev/null
	sudo fastboot flash bootloader $RESULT_DIR/u-boot.bin
	popd > /dev/null
}

function build_fastboot_boot()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo '# fastboot boot(Kernel)'
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	sleep 1.5
	pushd . > /dev/null
	sudo fastboot flash kernel $RESULT_DIR/uImage
	popd > /dev/null
}

function build_fastboot_system()
{
	echo ''
	echo ''
	echo '#########################################################'
	echo '#########################################################'
	echo '#'
	echo '# fastboot boot(system)'
	echo '#'
	echo '#########################################################'
	echo '#########################################################'

	sleep 1.5
	pushd . > /dev/null
	sudo fastboot flash ramdisk $RESULT_DIR/ramdisk.gz
	popd > /dev/null
}

function build_function_main()
{

	currentTime
	StartTime="${YEAR}-${MON}-${DAY} ${HOUR}:${MIN}:${SEC}"
	echo '#########################################################'
	echo "#  Build Time : "${StartTime}"                     #"
	echo '#########################################################'
	echo ""

	if [ -d $RESULT_DIR ]; then
		echo 'The result directory has already been created.'
	else
		echo 'Creating the result directory...'
		mkdir $RESULT_DIR
	fi

	if [ ${CMD_V_2NDBOOT} = "yes" ]; then
		CMD_V_BUILD_SEL="Make second boot"
		build_2ndboot
	fi

	if [ ${CMD_V_UBOOT} = "yes" ]; then
		CMD_V_BUILD_SEL="Build u-boot"
		build_uboot_source
	fi

	if [ ${CMD_V_KERNEL} = "yes" ]; then
		CMD_V_BUILD_SEL="Build Kernel"
		build_kernel_source
	fi

	if [ ${CMD_V_KERNEL_MODULE} = "yes" ]; then
		CMD_V_BUILD_SEL="Build Kernel module"
		build_kernel_module
	fi

	if [ ${CMD_V_APPLICATION} = "yes" ]; then
		CMD_V_BUILD_SEL="Build Application"
		build_application
	fi


	if [ ${CMD_V_BUILDROOT} = "yes" ]; then
		CMD_V_BUILD_SEL="Build Buildroot for filesystem"
		build_buildroot
	fi

	if [ ${CMD_V_FILESYSTEM} = "yes" ]; then
		CMD_V_BUILD_SEL="Build Filesystem"
		build_filesystem
	fi

	if [ -d ${RESULT_DIR} ]; then
		echo ""
		echo '#########################################################'
		echo "ls -al ${RESULT_DIR}"
		ls -al ${RESULT_DIR}
	fi

	currentTime
	EndTime="${YEAR}-${MON}-${DAY} ${HOUR}:${MIN}:${SEC}"

	echo ""
	echo '#########################################################'
	echo "#                 Complete success!"
	echo '#########################################################'
	echo "# Build Information"
	echo "#     Chipset       : "${CHIPSET_NAME}" "
	echo "#     Board Name    : "${BOARD_NAME}" "
	echo "#     U-boot Config : "${TEMP_UBOOT_TEXT}" "
	echo "#     Kernel Config : "${TEMP_KERNEL_TEXT}" "
	echo "#"
	echo "# Completion Time"
	echo "#     Start Time : "${StartTime}"	"
	echo "#     End Time   : "${EndTime}"	"
	echo '#########################################################'
}

################################################################
##
## main build start
##
################################################################

if [ -d $RESULT_DIR ]; then
	echo ""
else
	mkdir -p $RESULT_DIR
fi

if [ -f $RESULT_DIR/build.${CHIPSET_NAME}.uboot ]; then
	TEMP_UBOOT_TEXT=`cat $RESULT_DIR/build.${CHIPSET_NAME}.uboot`
fi
if [ -f $RESULT_DIR/build.${CHIPSET_NAME}.kernel ]; then
	TEMP_KERNEL_TEXT=`cat $RESULT_DIR/build.${CHIPSET_NAME}.kernel`
fi

if [ ${BOARD_NAME} != "build_exit" ]; then
	while [ -z $CMD_V_BUILD_NUM ]
	do
		clear
		echo "******************************************************************** "
		echo "[Build Function Menu]"
		echo "  TOP Directory : $TOP"
		echo "  Before Uboot  : ${TEMP_UBOOT_TEXT}"
		echo "  Before Kernel : ${TEMP_KERNEL_TEXT}"
		echo "  Board Name    : $BOARD_NAME"
		echo "  BOOT Device   : $BOOT_DEV"
		echo "******************************************************************** "
		echo "  1. ALL(+Compile)"
		echo "     1c. Clean Build"       
		echo " "
		echo "--------------------------------------------------------------------"
		echo "  2. 2ndboot+u-boot+kernel(+Build)   2c. Clean Build"
		echo "     21.  u-boot(+Build)		21c. u-boot(+Clean Build)"
		echo "     22.  kernel(+Build)		22c. kernel(+Clean Build)"
		echo "     23.  2ndboot(+Make)"       
		echo " "
		echo "     2m.  kernel menuconfig"
		echo "     2mc. ${KERNEL_CONFIG_NAME}_linux_defconfig -> .config"
		echo " "
		echo "--------------------------------------------------------------------"
		echo "  3. Application+Library(+Build)"
		echo "     3c. App+Lib(+Clean Build)"       
		echo " "
		echo "--------------------------------------------------------------------"
		echo "  4. Buildroot(+Build)"
		echo " "
		echo "--------------------------------------------------------------------"
		echo "  5. Ramdisk(+Make)"
		echo " "
		echo "--------------------------------------------------------------------"
		echo "  6. eMMC Packaging(All)"
		echo "     61. fastboot secondboot(2ndboot)"
		echo "     62. fastboot bootloader(u-boot)"
		echo "     63. fastboot boot(kernel)"
		echo "     64. fastboot system(rootfs)"
		echo " "
		echo "--------------------------------------------------------------------"
		echo "  0. Exit"
		echo "--------------------------------------------------------------------"

		echo -n "     Select Menu -> "
		read CMD_V_BUILD_NUM
		case $CMD_V_BUILD_NUM in
			#------------------------------------------------------------------------------------------------
			1) CMD_V_2NDBOOT=yes
				CMD_V_UBOOT=yes	
			    CMD_V_KERNEL=yes 
			    CMD_V_KERNEL_MODULE=yes
			    CMD_V_APPLICATION=yes
				CMD_V_BUILDROOT=yes
			    CMD_V_FILESYSTEM=yes					
			    ;;

				1c) CMD_V_2NDBOOT=yes
					CMD_V_UBOOT_CLEAN=yes
				    CMD_V_UBOOT=yes
				    CMD_V_KERNEL_CLEAN=yes
				    CMD_V_KERNEL=yes 
				    CMD_V_KERNEL_MODULE=yes
				    CMD_V_APPLICATION=yes
				    CMD_V_APPLICATION_CLEAN=yes
					CMD_V_BUILDROOT=yes
					CMD_V_BUILDROOT_CLEAN=yes
				    CMD_V_FILESYSTEM=yes
				    ;;

			#------------------------------------------------------------------------------------------------
			2) CMD_V_KERNEL=yes 
			    CMD_V_KERNEL_MODULE=yes
			    CMD_V_UBOOT=yes 
			    ;;
				2c) CMD_V_UBOOT=yes
					CMD_V_UBOOT_CLEAN=yes				
					CMD_V_KERNEL=yes 
					CMD_V_KERNEL_CLEAN=yes
					CMD_V_KERNEL_MODULE=yes
				    ;;
				21) CMD_V_UBOOT=yes							
				    ;;
				21c) CMD_V_UBOOT=yes
				     CMD_V_UBOOT_CLEAN=yes				
				     ;;
				22) CMD_V_KERNEL=yes 						
				     CMD_V_KERNEL_MODULE=yes
					 ;;
				22c) CMD_V_KERNEL=yes 
				     CMD_V_KERNEL_CLEAN=yes
					 CMD_V_KERNEL_MODULE=yes
				     ;;
				23) CMD_V_2NDBOOT=yes
					;;

				2m)	build_kernel_current_menuconfig	;;
				2mc)build_kernel_configcopy	;;

			#------------------------------------------------------------------------------------------------
			3)	CMD_V_APPLICATION=yes					
				;;
				3c) CMD_V_APPLICATION=yes
					CMD_V_APPLICATION_CLEAN=yes
					;;

			#------------------------------------------------------------------------------------------------
			4)	CMD_V_BUILDROOT=yes					;;

			#------------------------------------------------------------------------------------------------
			5)	CMD_V_FILESYSTEM=yes					;;

			#------------------------------------------------------------------------------------------------
			6)	CMD_V_BUILD_NUM=
				build_fastboot_2ndboot
				build_fastboot_uboot
				build_fastboot_boot
				build_fastboot_system					;;

				61)	CMD_V_BUILD_NUM=
					build_fastboot_2ndboot				;;
				62)	CMD_V_BUILD_NUM=
					build_fastboot_uboot				;;
				63)	CMD_V_BUILD_NUM=
					build_fastboot_boot				;;
				64)	CMD_V_BUILD_NUM=
					build_fastboot_system				;;

			#------------------------------------------------------------------------------------------------
			0)	CMD_V_BUILD_NUM=0
				echo ""
				exit 0										;;

			#------------------------------------------------------------------------------------------------
			*)	CMD_V_BUILD_NUM=							;;

		esac
		echo
	done

	if [ $CMD_V_BUILD_NUM != 0 ]; then
		CMD_V_LOG_FILE=$RESULT_DIR/build.log
		rm -rf CMD_V_LOG_FILE
		build_function_main 2>&1 | tee $CMD_V_LOG_FILE
	fi
fi

echo ""


