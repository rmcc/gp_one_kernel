#!/bin/sh --verbose
# Loads and unloads the driver modules in the proper sequence

# OUTPUT_DIR="$WORKAREA/host/art/sdio_driver/.output/ellenlinux/images/"
OUTPUT_DIR="bin/"

 if [ $1=="onebit" ]
 then
    ONEBIT=1
 else
    ONEBIT=0
 fi

case $1 in
    unloadall ) 
    echo "..unloading all"
    sudo /sbin/rmmod -f dksdio.ko
    sudo /sbin/rmmod -f sdio_pciellen_hcd.ko
    sudo /sbin/rmmod -f sdio_busdriver.ko
    sudo /sbin/rmmod -f sdio_lib.ko
    sudo rm -f /dev/dksdio0 
    ;;
    * ) 
    sudo mknod /dev/dksdio0 c 65 0
    sudo chmod 666 /dev/dksdio0
    sudo /sbin/insmod $OUTPUT_DIR/sdio_lib.ko
    sudo /sbin/insmod $OUTPUT_DIR/sdio_busdriver.ko
    sudo /sbin/insmod $OUTPUT_DIR/sdio_pciellen_hcd.ko
    sudo /sbin/insmod $OUTPUT_DIR/dksdio.ko 
    ;;
esac

  

