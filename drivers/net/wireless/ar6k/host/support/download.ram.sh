#!/bin/sh
# Downloads an application to target RAM

if [ -z "$WORKAREA" ]
then
	echo "Please set your WORKAREA environment variable."
	exit
fi

if [ -z "$ATH_PLATFORM" ]
then
	echo "Please set your ATH_PLATFORM environment variable."
	exit
fi

BMILOADER=${BMILOADER:-$WORKAREA/host/.output/$ATH_PLATFORM/image/bmiloader}
if [ ! -x "$BMILOADER" ]; then
	echo "Loader application '$BMILOADER' not found"
	exit
fi

if [ -z "$NETIF" ]
then
	NETIF=eth1
fi

eval export `$BMILOADER -i $NETIF --info | grep TARGET_TYPE`
eval export `$BMILOADER -i $NETIF --info | grep TARGET_VERSION`

case $TARGET_TYPE in
		AR6001)
			LOADADDR=0x80002000
			BEGINADDR=0x80002000
		;;
		AR6002)		
			TARGET_VERSION_REV1="0x20000086"
			if [ "$TARGET_VERSION" = "$TARGET_VERSION_REV1" ]
			then
				LOADADDR=0x502000
			else
				LOADADDR=0x502400
			fi
			BEGINADDR=0x915000
		;;
		*)
		echo "Check your TARGET_TYPE environment variable"
		exit
esac

if [ -e "$1" ]; then
        echo "Downloading file '$1' ..."
	$BMILOADER -i $NETIF -w -a $LOADADDR -f $1
	echo
        echo "Setting the start address ..."
	$BMILOADER -i $NETIF -b -a $BEGINADDR
else
	echo "File '$1' not found"
	echo "Usage: $0 <filename.bin>"
fi
