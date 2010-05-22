# Generates the phyregs executable and runs the executable to create 
# $WORKAREA/host/support/phyregs.bin

CC := $(ATH_CROSS_COMPILE_TYPE)gcc

all:  phyregs
	./phyregs

phyregs:
	$(CC) -Wall -I../../include -I../../../include phyregs.c -o phyregs

clean:
	rm phyregs
