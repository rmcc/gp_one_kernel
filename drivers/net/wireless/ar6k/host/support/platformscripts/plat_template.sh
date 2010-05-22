#!/bin/sh
#
# Platform-Dependent Template.
# This file should be re-name as plat_<ATH_PLATFORM>.sh
#    ATH_PLATFORM identifies the built and bus type, ie:
#       LOCAL_i686-SDIO
#
# The script arguments are:
#     
#   <operation> <image path for binaries>
#

IMAGEPATH=$2

echo "Image path: $IMAGEPATH"

case $1 in
	load)
    # TODO : start revEvent if desired
    #      $IMAGEPATH/recEvent $logfile > /dev/null 2>&1 &
	# TODO : load any bus dependent kernel modules here
	;;
	
	unload)
	# TODO: unload any bus dependent kernel modules
	;;
	
	loadAR6K)
	# load ar6k kernel module
	#     note: AR6K_MODULE_ARGS is exported by the top level script 
	#     note: AR6K_TGT_LOGFILE is exported by the top level script
	
	# TODO - start logging:
	#     $IMAGEPATH/recEvent $AR6K_TGT_LOGFILE /dev/null 2>&1 &
	
	/sbin/insmod $IMAGEPATH/ar6000.ko $AR6K_MODULE_ARGS
	;;
	unloadAR6K)
	echo "unloading AR6K module..."
	# TODO - kill logging:   
	#     killall recEvent
	
	/sbin/rmmod -w ar6000.ko
	;;
	*)
		echo "Unknown option : $1"
	
esac
