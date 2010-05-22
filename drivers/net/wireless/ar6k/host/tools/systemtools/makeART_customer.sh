#!/bin/sh

MDK_PATH=`pwd`
cd ../../..
export WORKAREA=`pwd`
export TARGET=AR6002
cd $MDK_PATH
make -f makefile.linux.customer clean all

