#!/bin/sh
# Load ART target image into RAM.
Help() {
	echo "Usage: $0 [options]"
	echo
	echo "With NO options, loads ART target image into RAM"
	echo
	echo "To unload, use $WORKAREA/host/support/loadAR6000.sh unloadall"
	exit 0
}

export NETIF=${NETIF:-eth1}
export EEPROM=${EEPROM:-$WORKAREA/host/support/fakeBoardData_AR6002.bin}
XTALFREQ=${XTALFREQ:-26000000}

LOADAR6000=${LOADAR6000:-$WORKAREA/host/support/loadAR6000.sh}
if [ ! -x "$LOADAR6000" ]; then
	echo "Loader application '$LOADAR6000' not found"
	exit
fi

BMILOADER=${BMILOADER:-$WORKAREA/host/.output/$ATH_PLATFORM/image/bmiloader}
if [ ! -x "$BMILOADER" ]; 
then
	echo "Loader application '$BMILOADER' not found"
	exit
fi

echo "Loading BMI only"
$LOADAR6000 hostonly

if [ "$TARGET_TYPE" = "" ]
then
    # Determine TARGET_TYPE
    eval export `$BMILOADER -i $NETIF --quiet --info | grep TARGET_TYPE`
fi
echo TARGET TYPE is $TARGET_TYPE

if [ "$TARGET_VERSION" = "" ]
then
    # Determine TARGET_VERSION
    eval export `$BMILOADER -i $NETIF --quiet --info | grep TARGET_VERSION`
fi
AR6002_VERSION_REV1=0x20000086
echo TARGET VERSION is $TARGET_VERSION

if [ "$TARGET_TYPE" = "AR6002" ]
then
    if [ "$TARGET_VERSION" = "$AR6002_VERSION_REV1" ]
    then
        export wlanapp=$WORKAREA/target/AR6002/hw1.0/bin/device.bin
    else
        export wlanapp=$WORKAREA/target/AR6002/hw2.0/bin/device.bin
    fi
fi

while [ "$#" -ne 0 ]
do
        case $1 in
	-h|--help )
		Help
		;;
       * )
      	echo "Unsupported argument"
            Help
		exit -1
		shift
	esac
done

echo "Loading ART target"
$LOADAR6000 targonly enableuartprint noresetok nostart bypasswmi 

echo "Setting XTALFREQ"
$BMILOADER -i $NETIF --write --address=0x500478 --param=$XTALFREQ
# TBD: set hi_board_data_initialized to avoid loading eeprom.
$BMILOADER -i $NETIF --write --address=0x500458 --param=0x1

sleep 1

