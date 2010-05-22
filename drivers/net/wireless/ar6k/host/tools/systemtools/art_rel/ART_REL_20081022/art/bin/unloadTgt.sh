#! /bin/bash

export WORKAREA=`pwd`
export ATH_PLATFORM=LOCAL_i686-SDIO
export TARGET_TYPE=AR6002

sudo $WORKAREA/host/support/loadAR6000.sh unloadall

