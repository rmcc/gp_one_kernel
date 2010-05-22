
NETIF=${NETIF:-eth1}

before=/tmp/test5.before
after=/tmp/test5.after

echo Running test5

bmiloader -i $NETIF --read --file=$before --length=32 --address=0x004f3000
cmp --silent $before test5.expected
if [ "$?" -eq 0 ]
then
  echo "ERROR: test5 patch contents in place before being applied"
  exit 1
fi

# Write new data
bmiloader -i $NETIF --write --file=test5.expected --address=0x0052d060

fwpatch --ifname=$NETIF --verbose --file=test5.rpdf
bmiloader -i $NETIF --read --file=$after --length=32 --address=0x004f3000
cmp --silent $after test5.expected
if [ "$?" -ne 0 ]
then
  echo "ERROR: test5 patches not applied"
  exit 1
fi

rm $before $after
exit 0
