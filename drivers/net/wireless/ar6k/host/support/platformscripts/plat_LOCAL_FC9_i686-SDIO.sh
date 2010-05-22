#!/bin/sh
#
# Platform-Dependent load script for x86 FC9 platform using Atheros SDIO stack
#

IMAGEPATH=$2

echo "Image path: $IMAGEPATH"

case $1 in
	load)
	echo "sdiostack loading"
	/sbin/insmod $IMAGEPATH/sdio_lib.ko
	/sbin/insmod $IMAGEPATH/sdio_busdriver.ko debuglevel=7 RequestListSize=300
	/sbin/insmod $IMAGEPATH/sdio_pcistd_hcd.ko debuglevel=7
	;;
	unload)
	echo "sdio stack unloading ..."	
	/sbin/rmmod -w sdio_pcistd_hcd.ko
	/sbin/rmmod -w sdio_busdriver.ko
	/sbin/rmmod -w sdio_lib.ko
	;;
	loadAR6K)
	echo "loading AR6K module... Args = ($AR6K_MODULE_ARGS) , logfile:$AR6K_TGT_LOGFILE"
	$IMAGEPATH/recEvent $AR6K_TGT_LOGFILE /dev/null 2>&1 &
	/sbin/insmod $IMAGEPATH/ar6000.ko $AR6K_MODULE_ARGS
	;;
	unloadAR6K)
	echo "unloading AR6K module..."
	/sbin/rmmod -w ar6000.ko
	killall recEvent
	;;
	*)
		echo "Unknown option : $1"
	
esac
