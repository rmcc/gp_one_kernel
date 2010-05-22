
NETIF=${NETIF:-eth1}

before=/tmp/test4.before
after=/tmp/test4.after

echo Running test4

bmiloader -i $NETIF --read --file=$before --length=2048 --address=0xa1030000
cmp --silent $before test4.expected
if [ "$?" -eq 0 ]
then
  echo "ERROR: test4 patch contents in place before being applied"
  exit 1
fi

fwpatch --ifname=$NETIF --verbose --file=test4.rpdf
bmiloader -i $NETIF --read --file=$after --length=2048 --address=0xa1030000
cmp --silent $after test4.expected
if [ "$?" -ne 0 ]
then
  echo "ERROR: test4 patches not applied"
  exit 1
fi

rm $before $after
exit 0
