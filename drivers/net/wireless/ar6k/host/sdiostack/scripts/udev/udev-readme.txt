To install udev on the Gumstix
1. Copy udev, udevinfo, udevinstall.scr and sdio-udev.rules to the gumstix /tmp dir.
2. run the udevinstall.scr script. It does the items listed below.
3. modify /etc/fstab. add the following line at the end:
/udev/sdmem0p1	/mnt/mmc	vfat	noauto			0	0
4. Change your module load scripts to remove any of the mknod support. The load modlues should just 
load the drivers.
5. To test:
#ls /udev
  should be empty
plug in an SD or MMC card. 
#ls /udev
  should have dev nodes for partitions
6. one time test for Bluetooth to develop the Bluetooth udev rules.
Find the Bluetooth device under /sys/class/??? or look around the /sys till you find it, then do:
#udevinfo -a -p /sys/path/to/hardware/info
  e-mail me the info from this list.

udevinstall.scrp script tasks:
1. note, must have kernel with SYSCTL and HOTPLUG enabled.
2. put udev in /sbin
3. set it as the hotplug program
#echo "/sbin/udev" > /proc/sys/kernel/hotplug
4. make directories
#mkdir /etc/udev
#mkdir /etc/udev/rules.d
5. copy udev rules
#cp sdio-udev.rules /etc/udev/rules.d
6. create directory where udev wil create device nodes
#mkdir /udev



