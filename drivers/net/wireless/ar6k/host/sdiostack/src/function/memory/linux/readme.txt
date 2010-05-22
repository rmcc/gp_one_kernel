To use the memory driver for SD and MMC cards:
1. Start the stack up. 
(Note, on my system you have to have the MMC/SD plugged in at boot,
or it often forces a re-boot. If yours doesn't do this with the BFC, then can you test
the unplug/plug case a few times - WARNING Linux expects you to umount before you 
pull the card.)
2. cat /proc/devices
Look under the Block devices: and what block major number is assigned to sdiommc.
Expected number is 254.
3. mknod /dev/sdiommc0p1 b 254 1
This is partition 1 of the disk.
4. mount -t vfat /dev/sdiommc0p1 /mnt/mmc
This mounts the disk read/write. If you have real data on the disk, 
mount it with the -r option. readonly, at first for saftey sake.
5. ls -l /mnt/mmc
to get a directory listing
df 
to see the file system parameters
6. umount /mnt/mmc
To unmount the card. There is a fair amount of lazy writing going on,
so failing to unmount may cause some bad things to happen.

Paul