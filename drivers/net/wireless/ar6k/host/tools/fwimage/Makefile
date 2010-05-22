CC=gcc
INCLUDES=
CFLAGS=-ggdb $(INCLUDES)
LDFLAGS=-ggdb

SOURCES=fwimage.c crc32.c

OBJS=fwimage.o crc32.o
	
DEPS := $(patsubst %.o,%.d,$(OBJS))

all:	deps fwimage

.PHONY:	all

fwimage: $(OBJS) Makefile
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

deps: $(SOURCES)
	$(CC) $(INCLUDES) -MD -E $(SOURCES) > /dev/null
     
clean:
	rm -f *~ *.o *.d
	
-include $(DEPS)
