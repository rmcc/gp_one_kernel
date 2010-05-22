wlan-dset-gen --gpio=<enable/disable> --pin=<pin # to be used for host wakeup>--fname=<filename for binary dataset file>

This generates a binary file containing the wlan-dset 
(say for example wow.bin).

Now use mkdsetimg to create an image from this bin file. 
You will need a dsets.txt file (see sample-dsets.txt).

mkdsetimg --desc=sample-dsets.txt --out=wow.dset --idxaddr=0x80000c00 --verbose

mkdsetimg will generate the wow.dset image file, now use bmiloader to write this file into target memory.

mkdsetimg spits out 2 important pieces of data which look like this -
Data starts at 0x80000bdc
Index is at 0x80000be0

When we use bmiloader, we will need both these addresses.

First get the location of the dset_RAM_index_table 
ramidx=`mipsisa32-elf-nm AR6kSDK.msawant_etna.9999/target/AR6000/image/ecos.flash.out | grep dset_RAM_index | cut -d ' ' -f 1`

Use bmiloader to write the image at the "data starts at ..." address.
bmiloader -i $NETIF --write --address=0x80000bdc --file=wow.dset

Use bmiloader to write the "index is at..." address to the dset_RAM_index_table
bmiloader -i $NETIF --write --address 0x$ramidx --param=0x80000be0

bmiloader -i $NETIF --done

Your dset is loaded in target memory and can now be accessed.
