#!/bin/sh

echo Install AR6002 LF Clock Calibration WAR

export IMAGEPATH=$WORKAREA/host/.output/$ATH_PLATFORM/image
export NETIF=${NETIF:-eth1}

# Step 1: Use remap entry 31 to nullify firmware's initialization
# of LPO_CAL_TIME at 0x8e1027
# Step 1a: Load modified text at end of memory
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfe0 --param=0x00239100
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfe4 --param=0xf0102282
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfe8 --param=0x23a20020
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfec --param=0x0008e09e
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dff0 --param=0xc0002441
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dff4 --param=0x24a20020
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dff8 --param=0x046a67b0
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dffc --param=0x63b20b0c
# Step 1b: Write remap size
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x80fc --param=0x0
# Step 1c: Write compare address
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x817c --param=0x4e1020
# Step 1d: Write target address
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x81fc --param=0x12dfe0
# Step 1e: Write valid bit
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x807c --param=0x1

# Step 2: Use remap entry 30 to nullify firmware's initialization
# of LPO_CAL_TIME at 0x8e12f7
# Step 2a: Load modified text at end of memory
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfc0 --param=0x00203130
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfc4 --param=0x220020c0
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfc8 --param=0x63f2b76e
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfcc --param=0x0c0a0c9e
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfd0 --param=0x21a4d20b
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfd4 --param=0xf00020c0
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfd8 --param=0x34c10020
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfdc --param=0x00359100
# Step 2b: Write remap size
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x80f8 --param=0x0
# Step 2c: Write compare address
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x8178 --param=0x4e12e0
# Step 2d: Write target address
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x81f8 --param=0x12dfc0
# Step 2e: Write valid bit
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x8078 --param=0x1

# Step 3: Use remap entry 29 to nullify firmware's initialization
# of host_interest at 0x8e1384
# Step 3a: Load modified text at end of memory
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfa0 --param=0x04a87685
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfa4 --param=0x1b0020f0
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfa8 --param=0x00029199
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfac --param=0x3dc0dd90
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfb0 --param=0x04ad76f0
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfb4 --param=0x1b0049a2
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfb8 --param=0xb19a0c99
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x52dfbc --param=0x8ce5003c
# Step 3b: Write remap size
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x80f4 --param=0x0
# Step 3c: Write compare address
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x8174 --param=0x4e1380
# Step 3d: Write target address
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x81f4 --param=0x12dfa0
# Step 3e: Write valid bit
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x8074 --param=0x1

# Step 4: Reset.
# Step 4a: Save state that may have been changed since reset.
save_sleep=`$IMAGEPATH/bmiloader -i $NETIF --get --address=0x40c4 | tail -n 1 | sed -e 's/.*: //'`
save_options=`$IMAGEPATH/bmiloader -i $NETIF --get --address=0x180c0 | tail -n 1 | sed -e 's/.*: //'`

# Step 4b: Issue reset.
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x4000 --param=0x100

# Step 4c: Sleep while reset occurs - TBDXXX
sleep 1

# Step 4d: Restore state that may have been changed since reset.
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x40c4 --param=$save_sleep
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x180c0 --param=$save_options

# Step 5: Reclaim remap entries 29 and 30, since they are no longer needed.
# (Could reclaim remap entry 31 later, if needed.)
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x8074 --param=0x0
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x8078 --param=0x0

# Step 6: Write LPO_INIT_DIVIDEND_INT.
# TBDXXX: This value is hardcoded for a 26MHz reference clock
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x40d8 --param=0x2e7ddb

# Step 7: Write LPO_INIT_DIVIDEND_FRACTION.
# TBDXXX: This value is hardcoded for a 26MHz reference clock
$IMAGEPATH/bmiloader -i $NETIF --set --address=0x40dc --param=0x0

echo Done installing AR6002 LF Clock calibration WAR

exit 0
