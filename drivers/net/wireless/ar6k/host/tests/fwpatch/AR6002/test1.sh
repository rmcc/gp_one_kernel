
NETIF=${NETIF:-eth1}

before=/tmp/test1.before
after=/tmp/test1.after

# Try test1.rpdf in each patch slot
patchid=31
while [ $patchid -ge 0 ]
do
  echo Running test1.$patchid

  bmiloader -i $NETIF --read --file=$before --length=32 --address=0x4f3000
  cmp --silent $before test1.expected
  if [ "$?" -eq 0 ]
  then
    echo "ERROR: test1.$patchid patch contents in place before being applied"
    exit 1
  fi
  fwpatch --ifname=$NETIF --verbose --file=test1.rpdf
  bmiloader -i $NETIF --read --file=$after --length=32 --address=0x4f3000
  cmp --silent $after test1.expected
  if [ "$?" -ne 0 ]
  then
    echo "ERROR: test1.$patchid patch not applied"
    exit 1
  fi

  # Deactivate, but do not uninstall
  rompatcher --ifname=$NETIF --deactivate --patchid=$patchid

  patchid=$(($patchid - 1))
done

rm $before $after
exit 0
