
NETIF=${NETIF:-eth1}

after=/tmp/test9.after

echo Running test9

# Clear RAM
bmiloader -i $NETIF --write --length=32 --file=test9.zeros --address=0x0052d060

fwpatch --ifname=$NETIF --verbose --file=test9.rpdf

bmiloader -i $NETIF --read --file=$after --length=64 --address=0x004f3000
cmp --silent $after test9.expected
if [ "$?" -ne 0 ]
then
  echo "ERROR: test9 patches not applied"
  exit 1
fi

rm $after
exit 0
