all: clean wlan-dset-gen

wlan-dset-gen: ../../../include/wlan_dset.h
	gcc -Wall -g -I../../include -I../../../include wlan_dset_gen.c -o wlan-dset-gen

clean:
	rm -rf wlan-dset-gen 
