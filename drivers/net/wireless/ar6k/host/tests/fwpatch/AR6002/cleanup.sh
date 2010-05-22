
NETIF=${NETIF:-eth1}

# Deactivate & uninstall all patches
patchid=31
while [ $patchid -ge 0 ]
do
  echo Deactivate and Uninstall patchid $patchid
  rompatcher --ifname=$NETIF --deactivate --uninstall --patchid=$patchid
  patchid=$(($patchid - 1))
done

