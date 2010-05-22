# Copyright 2004-2006 Atheros Communications Inc.
# 
#
#The software source and binaries included in this development package are
#licensed, not sold. You, or your company, received the package under one
#or more license agreements. The rights granted to you are specifically
#listed in these license agreement(s). All other rights remain with Atheros
#Communications, Inc., its subsidiaries, or the respective owner including
#those listed on the included copyright notices.  Distribution of any
#portion of this package must be in strict compliance with the license
#agreement(s) terms.
#
#
#
# EMSDIO master make file
#
# Minimal build invocation:
#
#   make ALL_BUILD=1 CT_BUILD_TYPE=<build type> CT_OS_TYPE=<Operating System> default | clean
#
# Notes:
#     1. This makefile must be invoked from the host/ directory
#     2. The <build type> must match an entry in localmake.$(CT_OS_TYPE).inc.
#     3. The localmake.$(CT_OS_TYPE).inc can be overridden using an include file outside the build tree.
#        This file (name and path) can be set via the CT_MAKE_INCLUDE_OVERRIDE variable.
#        ** If CT_MAKE_INCLUDE_OVERRIDE is used, you can define all build variables in that file 
#        instead of using command line arguments **.  This feature is provided for developers
#        that may want to customize the build using a single include file.
#
#        For example :
#             
#            " make CT_MAKE_INCLUDE_OVERRIDE=$HOME/mymake.inc "
#        
#             could be used, as long as "mymake.inc" defines all the required variables (see below)
  
#  Required Variables:  
#      
#      CT_BUILD_TYPE  - build type defined in localmake.linux.inc, this variable is also used 
#                       to build a path to hold compiled binaries.
#      CT_OS_TYPE - operating system type (linux, etc...)
#	   CT_OS_SUB_TYPE - on linux, this must be "linux_2_4" for 2.4 kernels or left blank for 2.6 kernels.
#      CT_LINUXPATH - linux kernel source path
#      CT_CROSS_COMPILE_TYPE - optional cross compiler path 
#      CT_ARCH_CPU_TYPE  - CPU architecture type
#
#      One of the following should be used to build the tree:
#      ALL_BUILD=1 - build all source code
#      HDK_BUILD=1 - build only HCD and sample function drivers
#      PDK_BUILD=1 - build only the sample function drivers
#      BUS_BUILD=1 - build bus driver, library and HCDs for use with built-in SDIO cards.
#
# Override variables:
#
#    CT_MAKE_INCLUDE_OVERRIDE - full path to include file which overrides the default
#                               this file can contain other overrides specific to a developer's
#                               workspace environment.
#    CT_BUILD_OUTPUT_OVERRIDE - output path override for compiled binaries.
#

#include local variables

#+++FIH+++
CT_OS_TYPE=linux
CT_OS_SUB_TYPE=linux_2_6
#---FIH---

ifdef CT_MAKE_INCLUDE_OVERRIDE
-include $(CT_MAKE_INCLUDE_OVERRIDE)
else
-include localmake.$(CT_OS_TYPE).inc
-include localmake.$(CT_OS_TYPE).private.inc
endif

export CT_OS_TYPE
export CT_OS_SUB_TYPE
export CT_OS_TOP_LEVEL_RULE 
export CT_PASS_CFLAGS
export CT_SRC_BASE
export CT_BUILD_SUB_PROJ

# this makefile can only be invoked from the /EMSDIO/src base
CT_SRC_BASE :=$(shell pwd)
 
# export flags for which HCDs to build. Set the hcd driver name in hcd/ in your localmake.*.inc file.
export CT_HC_DRIVERS
export PDK_BUILD
export HDK_BUILD
export ALL_BUILD
export ATHRAW_FD_BUILD
export BUS_BUILD

# For Linux
ifeq ($(CT_OS_TYPE),linux) 

#make a copy for linux 2.4
EXTRA_CFLAGS += -DLINUX -I$(CT_SRC_BASE)/include 
ifneq ($(CT_RELEASE),1)
EXTRA_CFLAGS += -DDEBUG
endif  
export EXTRA_CFLAGS 

CT_SRC_OUTPUT :=$(CT_SRC_BASE)/../output

ifdef CT_BUILD_OUTPUT_OVERRIDE
_CT_COMPILED_OBJECTS_PATH :=$(CT_BUILD_OUTPUT_OVERRIDE)
_MAKE_OUTPUT_DIR :=
_CLEAN_OUTPUT_DIR :=
else
_CT_COMPILED_OBJECTS_PATH := $(CT_SRC_OUTPUT)/$(CT_BUILD_TYPE)
_MAKE_OUTPUT_DIR := mkdir --parents $(_CT_COMPILED_OBJECTS_PATH)
_CLEAN_OUTPUT_DIR := rm -R -f $(CT_SRC_OUTPUT)
endif

ifeq ($(CT_OS_SUB_TYPE),linux_2_4)

CT_PASS_CFLAGS := $(EXTRA_CFLAGS)
_CT_MOD_EXTENSION :=o
ifeq ($(ALL_BUILD),1)
subdir-m += busdriver/ lib/ hcd/ function/   
else 
ifeq ($(BUS_BUILD),1)
subdir-m += busdriver/ lib/ hcd/
else
ifeq ($(PDK_BUILD),1)
subdir-m += function/
else
ifeq ($(HDK_BUILD),1)
subdir-m += hcd/ function/ 
endif
endif
endif
endif

# add in rules to make modules
CT_OS_TOP_LEVEL_RULE :=$(CT_LINUXPATH)/Rules.make
include $(CT_OS_TOP_LEVEL_RULE)
else

#+++FIH+++ 
obj-y += lib/  
obj-y += busdriver/
obj-y += hcd/qualcomm/
#---FIH---


endif

ifdef CT_BUILD_SUB_PROJ
_CT_SUBDIRS=$(CT_BUILD_SUB_PROJ)
else
_CT_SUBDIRS=$(CT_SRC_BASE)
endif

ifdef CT_CROSS_COMPILE_TYPE 
CT_MAKE_COMMAND_LINE=$(CT_OUTPUT_FLAGS) -C $(CT_LINUXPATH) SUBDIRS=$(_CT_SUBDIRS) ARCH=$(CT_ARCH_CPU_TYPE) CROSS_COMPILE=$(CT_CROSS_COMPILE_TYPE)
else   
CT_MAKE_COMMAND_LINE=$(CT_OUTPUT_FLAGS) -C $(CT_LINUXPATH) SUBDIRS=$(_CT_SUBDIRS)
endif


endif
 


