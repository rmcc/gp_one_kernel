
CC := $(ATH_CROSS_COMPILE_TYPE)gcc

all: bdiff.c md5.o
	$(CC) -O2 -Wno-parentheses md5.o bdiff.c -o bdiff
	$(ATH_CROSS_COMPILE_TYPE)strip bdiff

md5.o:
	$(CC) -O2 -c md5.c -o md5.o

clean:
	rm -f bdiff md5.o
