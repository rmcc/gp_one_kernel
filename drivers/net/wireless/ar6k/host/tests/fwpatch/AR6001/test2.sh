
NETIF${NETIF:-eth1}

before=/tmp/test2.before
after=/tmp/test2.after

echo Running test2

bmiloader -i $NETIF --read --file=$before --length=16 --address=0xa1030000
cmp --silent $before test2.expected.0
if [ "$?" -eq 0 ]
then
  echo "ERROR: test2.0 patch contents in place before being applied"
  exit 1
fi

bmiloader -i $NETIF --read --file=$before --length=16 --address=0xa1030010
cmp --silent $before test2.expected.1
if [ "$?" -eq 0 ]
then
  echo "ERROR: test2.1 patch contents in place before being applied"
  exit 1
fi

bmiloader -i $NETIF --read --file=$before --length=32 --address=0xa1030020
cmp --silent $before test2.expected.2
if [ "$?" -eq 0 ]
then
  echo "ERROR: test2.2 patch contents in place before being applied"
  exit 1
fi

fwpatch --ifname=$NETIF --verbose --file=test2.rpdf
bmiloader -i $NETIF --read --file=$after --length=64 --address=0xa1030000
cat test2.expected.0 test2.expected.1 test2.expected.2 | cmp --silent $after
if [ "$?" -ne 0 ]
then
  echo "ERROR: test2 patches not applied"
  exit 1
fi

rm $before $after
exit 0
