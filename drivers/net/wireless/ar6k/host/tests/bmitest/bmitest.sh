
if [ -z "$WORKAREA" ]; then
	echo "Please set you WORKAREA enviroment variable"
	exit 1
fi

if [ -z "$ATH_PLATFORM" ]; then
	echo "Please set you ATH_PLATFORM enviroment variable"
	exit 1
fi

if [ -z "$TARGET_TYPE" ]; then
 	echo "Please set you TARGET_TYPE enviroment variable"
 	exit 1
fi

BMILOADER=${BMILOADER:-$WORKAREA/host/.output/$ATH_PLATFORM/image/bmiloader}
WMICONFIG=${WMICONFIG:-$WORKAREA/host/.output/$ATH_PLATFORM/image/wmiconfig}
NETIF=${NETIF:-eth2}
LOGFILE=/dev/null

if [ ! -x "$BMILOADER" ]; then
	echo "bmiloader not found,'$BMILOADER'"
	exit 1
fi

if [ ! -x "$WMICONFIG" ]; then
	echo "wmiconfig not found,'$WMILOADER'"
	exit 1
fi

if [ "$TARGET_TYPE" == "AR6002" ]; then
	REG_ADDR=0x180c0              # LOCAL_SCRATCH_REGISTER
	RAM_ADDR=0x516000             # RAM
	BMI_EXEC_FN=0x915068          # address of bmi_exec_test()
	VAL_ADDR=0x505010             # address of UsrCmd
	BMI_APP_START_FN=0x915070     # address of bmi_app_start_test()
fi
if [ "$TARGET_TYPE" == "AR6001" ]; then
	REG_ADDR=0x0c0140c0           # LOCAL_SCRATCH_REGISTER
	RAM_ADDR=0x80002000           # RAM
	BMI_EXEC_FN=0x8200b0dc        # address of bmi_exec_test()
	VAL_ADDR=0x800020bc           # address of UsrCmd
	BMI_APP_START_FN=0x8200b0e4   # address of bmi_app_start_test()
fi

# Test 1: BMI read and write SOC register test
# reads and writes the LOCAL_SCRATCH_REGISTER 
OLD_REG_VAL=0
SET_REG_VAL=0xf
GET_REG_VAL=0
OLD_REG_VAL=`$BMILOADER -i $NETIF --get --address=$REG_ADDR | grep Return | cut -d ' ' -f 5`
$BMILOADER -i $NETIF --set --address=$REG_ADDR --param=$SET_REG_VAL > $LOGFILE
GET_REG_VAL=`$BMILOADER -i $NETIF --get --address=$REG_ADDR | grep Return | cut -d ' ' -f 5`
if [ "$GET_REG_VAL" != "$SET_REG_VAL" ]; then
	echo "1: bmi read & write SOC register test failed"
else
	echo "1: passed"
fi
$BMILOADER -i $NETIF --set --address=$REG_ADDR --param=$OLD_REG_VAL > $LOGFILE

# Test 2: BMI read and write memory test
IN_FILE=test2.bin
OUT_FILE=/tmp/out.bin
FILE_SZ=`ls -l $IN_FILE | cut -d ' ' -f 6`
$BMILOADER -i $NETIF --write --address=$RAM_ADDR --file=$IN_FILE > $LOGFILE
$BMILOADER -i $NETIF --read --address=$RAM_ADDR --length=$FILE_SZ --file=$OUT_FILE > $LOGFILE
cmp --silent $IN_FILE $OUT_FILE
if [ "$?" -ne 0 ]; then
	echo "2: bmi read & write memory test failed"
else
	echo "2: passed"
fi

# Test 3: BMI execute test
FN_ARG=0xff
FN_RET=0
FN_EXP=0xfe
FN_RET=`$BMILOADER -i $NETIF --execute --address=$BMI_EXEC_FN --param=$FN_ARG | grep Return | cut -d ' ' -f 5`
if [ "$FN_RET" != "$FN_EXP" ]; then
	echo "3: bmi execute test failed"
else
	echo "3: passed"
fi

# Test 4: BMI set app start test
VAL_EXP=0x12345678
VAL_RET=0
$BMILOADER -i $NETIF --begin --address=$BMI_APP_START_FN > $LOGFILE
$BMILOADER -i $NETIF -d > $LOGFILE
VAL_RET=`$WMICONFIG -i $NETIF --diagread --diagaddr=$VAL_ADDR | grep diagdata | cut -d ' ' -f 2`
if [ "$VAL_RET" != "$VAL_EXP" ]; then
	echo "4: bmi set app start test failed"
else
	echo "4: passed"
fi

exit 0
