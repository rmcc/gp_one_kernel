CC	= cc
#CFLAGS	= -O
CFLAGS	= -g

TARGETS = pgen pget phone ap upsd staut apts APSDSniff

# Compute RELNAME from current directory (pgenYYMMDD)
RELNAME = $(shell basename `pwd`)

all:	$(TARGETS)

.PHONY: APSDSniff

APSDSniff:
	$(MAKE) -C APSDSniff

pgen:	pgen.c mpx.o
	$(CC) $(CFLAGS) -o pgen pgen.c mpx.o

upsd:	pgen.c mpx.o
	$(CC) $(CFLAGS) -o upsd pgen.c mpx.o

pget:	pgen.c mpx.o
	$(CC) $(CFLAGS) -o pget pgen.c mpx.o

phone:	pgen.c mpx.o
	$(CC) $(CFLAGS) -o phone pgen.c mpx.o

ap:	pgen.c mpx.o
	$(CC) $(CFLAGS) -o ap pgen.c mpx.o

apts:	pgen.c mpx.o
	$(CC) $(CFLAGS) -o apts pgen.c mpx.o

staut:	pgen.c mpx.o
	$(CC) $(CFLAGS) -o staut pgen.c mpx.o

mpx.o:  mpx.c
	$(CC) $(CFLAGS) -c mpx.c

clean:	
	rm -f *.o *~ *.bak $(TARGETS) $(TARGETS:=.exe)
	$(MAKE) -C APSDSniff clean

dist: clean
	rm -f ../$(RELNAME).zip
	cd ..; zip -qr $(RELNAME).zip $(RELNAME)
	@echo Created $(RELNAME).zip
