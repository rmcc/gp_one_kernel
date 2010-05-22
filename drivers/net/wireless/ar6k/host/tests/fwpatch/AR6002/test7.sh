
NETIF=${NETIF:-eth1}

after=/tmp/test7.after

echo Running test7

# Clear RAM
bmiloader -i $NETIF --write --length=128 --file=test7.zeros --address=0x0052d000

fwpatch --ifname=$NETIF --verbose --file=test7.rpdf

bmiloader -i $NETIF --read --file=$after --length=64 --address=0x004f3000
cmp --silent $after test7.expected.1
if [ "$?" -ne 0 ]
then
  echo "ERROR: test7 patches not applied"
  exit 1
fi

# verify that all RAM bytes were written properly
bmiloader -i $NETIF --read --file=$after --length=128 --address=0x0052d000
cmp --silent $after test7.expected.2
if [ "$?" -ne 0 ]
then
  echo "ERROR: test7 RAM not written"
  exit 1
fi

rm $after
exit 0
