
NETIF=${NETIF:-eth1}

before=/tmp/test3.before
after=/tmp/test3.after

echo Running test3

bmiloader -i $NETIF --read --file=$before --length=1024 --address=0x004f3000
cmp --silent $before test3.expected
if [ "$?" -eq 0 ]
then
  echo "ERROR: test3 patch contents in place before being applied"
  exit 1
fi

fwpatch --ifname=$NETIF --verbose --file=test3.rpdf
bmiloader -i $NETIF --read --file=$after --length=1024 --address=0x004f3000
cmp --silent $after test3.expected
if [ "$?" -ne 0 ]
then
  echo "ERROR: test3 patches not applied"
  exit 1
fi

rm $before $after
exit 0
