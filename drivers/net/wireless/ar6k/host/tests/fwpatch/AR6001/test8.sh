
NETIF=${NETIF:-eth1}

before=/tmp/test8.before
after=/tmp/test8.after

echo Running test8

bmiloader -i $NETIF --read --file=$before --length=16 --address=0xa1030000
cmp --silent $before test8.expected.0
if [ "$?" -eq 0 ]
then
  echo "ERROR: test8.0 patch contents in place before being applied"
  exit 1
fi

bmiloader -i $NETIF --read --file=$before --length=16 --address=0xa1030010
cmp --silent $before test8.expected.1
if [ "$?" -eq 0 ]
then
  echo "ERROR: test8.1 patch contents in place before being applied"
  exit 1
fi

bmiloader -i $NETIF --read --file=$before --length=32 --address=0xa1030020
cmp --silent $before test8.expected.2
if [ "$?" -eq 0 ]
then
  echo "ERROR: test8.2 patch contents in place before being applied"
  exit 1
fi

fwpatch --ifname=$NETIF --verbose --file=test8.rpdf
bmiloader -i $NETIF --read --file=$after --length=64 --address=0xa1030000
cat test8.expected.0 test8.expected.1 test8.expected.2 | cmp --silent $after
if [ "$?" -ne 0 ]
then
  echo "ERROR: test8 patches not applied"
  exit 1
fi

rm $before $after
exit 0
