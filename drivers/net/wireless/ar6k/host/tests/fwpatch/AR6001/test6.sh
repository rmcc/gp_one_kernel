
NETIF=${NETIF:-eth1}

before=/tmp/test6.before
after=/tmp/test6.after

echo Running test6

bmiloader -i $NETIF --read --file=$before --length=16 --address=0xa1030000
cmp --silent $before test6.expected.0
if [ "$?" -eq 0 ]
then
  echo "ERROR: test6.0 patch contents in place before being applied"
  exit 1
fi

bmiloader -i $NETIF --read --file=$before --length=16 --address=0xa1030010
cmp --silent $before test6.expected.1
if [ "$?" -eq 0 ]
then
  echo "ERROR: test6.1 patch contents in place before being applied"
  exit 1
fi

bmiloader -i $NETIF --read --file=$before --length=32 --address=0xa1030020
cmp --silent $before test6.expected.2
if [ "$?" -eq 0 ]
then
  echo "ERROR: test6.2 patch contents in place before being applied"
  exit 1
fi

fwpatch --ifname=$NETIF --verbose --file=test6.rpdf
bmiloader -i $NETIF --read --file=$after --length=64 --address=0xa1030000
cat test6.expected.0 test6.expected.1 test6.expected.2 | cmp --silent $after
if [ "$?" -ne 0 ]
then
  echo "ERROR: test6 patches not applied"
  exit 1
fi

rm $before $after
exit 0
