
# common environment variable for building driver
export PATH=/na_test2/arm-2006q3/bin:${PATH}
export ATH_BUILD_TYPE=QUALCOMM_ARM
export ATH_BSP_TYPE=MSM7201A_BSP6320

# for creating convenient package
export PLATFORM_NAME=${ATH_BUILD_TYPE}-SDIO
export DOWNLOAD_TOP=ath_top
export DOWNLOAD_TARGET=${DOWNLOAD_TOP}/target
export DOWNLOAD_HOST=${DOWNLOAD_TOP}/host
export DOWNLOAD_SUPPORT=${DOWNLOAD_HOST}/support
export DOWNLOAD_OUTPUT=${DOWNLOAD_HOST}/.output/${PLATFORM_NAME}/image
export DOWNLOAD_HW1_0=${DOWNLOAD_TARGET}/AR6002/hw1.0/bin
export DOWNLOAD_HW2_0=${DOWNLOAD_TARGET}/AR6002/hw2.0/bin
export DRIVER_SUPPOR=support
export DRIVER_OTHER_TOOLS=wifitools
export DRIVER_OUTPUT=.output/${PLATFORM_NAME}/image
export DRIVER_HW1_0=../target/AR6002/hw1.0/bin
export DRIVER_HW2_0=../target/AR6002/hw2.0/bin

while [ "$#" -eq 0 ]
do
	echo "Need argument"
	exit -1
done

case $1 in
	1)
		make
		cp sdiostack/src/hcd/qualcomm/sdio_qualcomm_hcd.ko .output/${ATH_BUILD_TYPE}-SDIO/image/
		rm -rf /qcroot/${DOWNLOAD_TOP}/host/.output
		tar cf - .output/ | (cd /qcroot/${DOWNLOAD_TOP}/host/ ; tar xf -)
        	;;
	2)
		make clean
		rm -rf .output/${PLATFORM_NAME}/image/*
		rm -rf ath_top_1.tgz ath_top_2.tgz
		rm -rf ${DOWNLOAD_TOP}
        	;;
	3)
		echo "Create binary folder \""${DOWNLOAD_TOP}"\" for hardware 1.0"
		# remove old folder
		rm -rf ${DOWNLOAD_TOP}
		rm -rf ath_top_1.tgz
		# create folder structure
		mkdir -p ${DOWNLOAD_SUPPORT}
		mkdir -p ${DOWNLOAD_OUTPUT}
		mkdir -p ${DOWNLOAD_HW1_0}
		# copy files
		cp ${DRIVER_HW1_0}/athwlan.bin ${DOWNLOAD_HW1_0}
		cp ${DRIVER_HW1_0}/athtcmd_ram.bin ${DOWNLOAD_HW1_0}
		cp ${DRIVER_HW1_0}/data.patch.hw1_0.bin ${DOWNLOAD_HW1_0}
		cp ${DRIVER_SUPPOR}/loadAR6000.sh ${DOWNLOAD_SUPPORT}
		cp ${DRIVER_SUPPOR}/loadtestcmd.sh ${DOWNLOAD_SUPPORT}
		cp ${DRIVER_SUPPOR}/AR6002.war1.sh ${DOWNLOAD_SUPPORT}
		cp ${DRIVER_OUTPUT}/*.ko ${DOWNLOAD_OUTPUT}
		cp ${DRIVER_OUTPUT}/bmiloader ${DOWNLOAD_OUTPUT}
		cp ${DRIVER_OUTPUT}/wmiconfig ${DOWNLOAD_OUTPUT}
		cp ${DRIVER_OUTPUT}/athtestcmd ${DOWNLOAD_OUTPUT}
		cp ${DRIVER_OUTPUT}/eeprom.AR6002 ${DOWNLOAD_OUTPUT}
		cp ${DRIVER_OTHER_TOOLS}/* ${DOWNLOAD_TOP}
		tar czvf ath_top_1.tgz ${DOWNLOAD_TOP}
        	;;
	4)
		echo "Create binary folder \""${DOWNLOAD_TOP}"\" for hardware 2.0"
		# remove old folder
		rm  -rf ${DOWNLOAD_TOP}
		rm -rf ath_top_2.tgz
		# create folder structure
		mkdir -p ${DOWNLOAD_SUPPORT}
		mkdir -p ${DOWNLOAD_OUTPUT}
		mkdir -p ${DOWNLOAD_HW2_0}
		# copy files
		cp ${DRIVER_HW2_0}/athwlan.bin ${DOWNLOAD_HW2_0}
		cp ${DRIVER_HW2_0}/athwlan.bin.z77 ${DOWNLOAD_HW2_0}
		cp ${DRIVER_HW2_0}/athtcmd_ram.bin ${DOWNLOAD_HW2_0}
		cp ${DRIVER_HW2_0}/data.patch.hw2_0.bin ${DOWNLOAD_HW2_0}
		cp ${DRIVER_SUPPOR}/loadAR6000.sh ${DOWNLOAD_SUPPORT}
		cp ${DRIVER_SUPPOR}/loadtestcmd.sh ${DOWNLOAD_SUPPORT}
		cp ${DRIVER_SUPPOR}/AR6002.war1.sh ${DOWNLOAD_SUPPORT}
		cp ${DRIVER_OUTPUT}/*.ko ${DOWNLOAD_OUTPUT}
		cp ${DRIVER_OUTPUT}/bmiloader ${DOWNLOAD_OUTPUT}
		cp ${DRIVER_OUTPUT}/athtestcmd ${DOWNLOAD_OUTPUT}
		cp ${DRIVER_OUTPUT}/wmiconfig ${DOWNLOAD_OUTPUT}
		cp ${DRIVER_OUTPUT}/eeprom.AR6002 ${DOWNLOAD_OUTPUT}
		cp ${DRIVER_OTHER_TOOLS}/* ${DOWNLOAD_TOP}
		tar czvf ath_top_2.tgz ${DOWNLOAD_TOP}
		;;
	*)
		echo "Unsupported argument"
		exit -1
esac


